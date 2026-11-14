"""
Real video downloader bridge for Cloudar Browser (tm)

Content scripts (e.g. the "Video Downloader" extension) cannot download
YouTube videos with a plain <a download> click because YouTube serves
video through MSE (blob: URLs / segmented streams), not a single file URL.

This module exposes a QObject over QWebChannel so a content script running
on the actual page (youtube.com, etc.) can ask the Python side to perform
a *real* download using yt-dlp - no network request is made from the page
itself, so this is unaffected by the site's Content-Security-Policy.

Registration (already wired in core/browser_window.py):
    self.youtube_downloader_bridge = YoutubeDownloaderBridge(self)
    self.web_channel.registerObject("youtubeDownloader", self.youtube_downloader_bridge)
"""
import json
import os
import re
import shutil
import threading

from core.browser_qt import QObject, pyqtSignal, pyqtSlot
from core.browser_resources import SETTINGS_FILE, DEFAULT_SETTINGS, load_json_file

# Quality tiers offered in the popup UI. Each maps to a yt-dlp format
# selector string; "height" is used to pick a matching video-only format
# when estimating file size (None for the audio-only tier).
QUALITY_TIERS = [
    {"key": "1080p", "label": "1080p", "height": 1080,
     "selector": "bestvideo[height<=1080]+bestaudio/best[height<=1080]"},
    {"key": "720p", "label": "720p", "height": 720,
     "selector": "bestvideo[height<=720]+bestaudio/best[height<=720]"},
    {"key": "360p", "label": "360p", "height": 360,
     "selector": "bestvideo[height<=360]+bestaudio/best[height<=360]"},
    {"key": "audio", "label": "Audio only", "height": None,
     "selector": "bestaudio/best"},
]


def _sanitize_filename(name):
    name = (name or "video").strip()
    name = re.sub(r'[\\/:*?"<>|]', "_", name)
    return name[:150] if name else "video"


def _fmt_size(num_bytes):
    if not num_bytes or num_bytes <= 0:
        return None
    units = ["B", "KB", "MB", "GB"]
    size = float(num_bytes)
    i = 0
    while size >= 1024 and i < len(units) - 1:
        size /= 1024
        i += 1
    return f"{size:.1f} {units[i]}"


def _yt_dlp_python_available():
    try:
        import yt_dlp  # noqa: F401
        return True
    except ImportError:
        return False


def _yt_dlp_cli_available():
    return shutil.which("yt-dlp") is not None


class YoutubeDownloaderBridge(QObject):
    """Exposed to web pages as `youtubeDownloader` via QWebChannel."""

    downloadStarted = pyqtSignal(str)               # url
    downloadProgress = pyqtSignal(str, float, str)  # url, percent, speed_str
    downloadFinished = pyqtSignal(str, bool, str)   # url, success, path_or_error
    videoInfoReady = pyqtSignal(str, str)           # url, json(info)
    videoInfoFailed = pyqtSignal(str, str)          # url, error

    def __init__(self, parent=None):
        super().__init__(parent)
        self._active = set()
        self._info_requests = set()

    # ── Called from JS ──────────────────────────────────────────────
    @pyqtSlot(result=bool)
    def isAvailable(self):
        """Whether a real download can be performed right now."""
        return _yt_dlp_python_available() or _yt_dlp_cli_available()

    @pyqtSlot(str)
    def requestVideoInfo(self, url):
        """Fetch title/thumbnail/duration + per-quality size estimates for
        `url` without downloading anything, and emit videoInfoReady (or
        videoInfoFailed) when done. Runs in a background thread since it
        needs a network round-trip to YouTube."""
        if not url or url in self._info_requests:
            return
        if not _yt_dlp_python_available():
            self.videoInfoFailed.emit(url, "yt-dlp chưa được cài. Chạy: pip install yt-dlp")
            return
        self._info_requests.add(url)
        threading.Thread(target=self._run_info_lookup, args=(url,), daemon=True).start()

    @pyqtSlot(str, str, str, str)
    def downloadVideo(self, url, suggested_title="", quality="", container=""):
        """Start downloading `url` at the requested `quality` tier
        ("1080p"/"720p"/"360p"/"audio", empty = best available) and
        `container` ("mp4"/"webm"/"mkv"/"" = keep the source's native
        container, no forced re-mux). Honors the browser's "Ask before
        download" (ask_download) setting: if enabled, shows a save dialog
        on the GUI thread first, exactly like normal browser downloads."""
        if not url or url in self._active:
            return

        out_dir = self._get_download_dir()
        chosen_stem = None  # None = let yt-dlp name the file from its title
        is_audio = quality == "audio"

        settings = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)
        if settings.get("ask_download", False):
            from core.browser_qt import QFileDialog

            if is_audio:
                ext, filt = ".m4a", "Audio (*.m4a *.webm *.opus);;Tất cả tệp (*.*)"
            elif container in ("webm", "mkv"):
                ext, filt = "." + container, f"Video ({container.upper()}) (*.{container});;Tất cả tệp (*.*)"
            else:
                ext, filt = ".mp4", "Video MP4 (*.mp4);;WebM (*.webm);;MKV (*.mkv);;Tất cả tệp (*.*)"
            default_name = _sanitize_filename(suggested_title) + ext
            default_path = os.path.join(out_dir, default_name)
            path, _ = QFileDialog.getSaveFileName(
                None, "Lưu video YouTube", default_path, filt,
            )
            if not path:
                self.downloadFinished.emit(url, False, "Đã hủy tải xuống")
                return
            out_dir = os.path.dirname(path) or out_dir
            chosen_stem = os.path.splitext(os.path.basename(path))[0]
            # If the user typed/kept a specific extension in the save
            # dialog, honor it as the forced container (unless audio).
            typed_ext = os.path.splitext(path)[1].lstrip(".").lower()
            if not is_audio and typed_ext in ("mp4", "webm", "mkv"):
                container = typed_ext

        self._active.add(url)
        threading.Thread(
            target=self._run_download,
            args=(url, suggested_title, out_dir, chosen_stem, quality, container),
            daemon=True,
        ).start()

    # ── Internal ─────────────────────────────────────────────────────
    def _get_download_dir(self):
        settings = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)
        path = settings.get("download_location") or DEFAULT_SETTINGS["download_location"]
        path = os.path.expanduser(path)
        try:
            os.makedirs(path, exist_ok=True)
        except Exception:
            pass
        return path

    @staticmethod
    def _format_selector_for(quality):
        """Map a quality tier key ("1080p"/"720p"/"360p"/"audio"/"") to a
        yt-dlp -f selector string."""
        for tier in QUALITY_TIERS:
            if tier["key"] == quality:
                return tier["selector"]
        return "bestvideo+bestaudio/best"  # "" or unknown -> best available

    def _run_download(self, url, suggested_title, out_dir, chosen_stem=None, quality="", container=""):
        self.downloadStarted.emit(url)
        try:
            if _yt_dlp_python_available():
                self._download_with_python_module(url, out_dir, chosen_stem, quality, container)
            elif _yt_dlp_cli_available():
                self._download_with_cli(url, out_dir, chosen_stem, quality, container)
            else:
                raise RuntimeError(
                    "yt-dlp chưa được cài. Chạy: pip install yt-dlp"
                )
        except Exception as e:
            self.downloadFinished.emit(url, False, str(e))
        finally:
            self._active.discard(url)

    def _download_with_python_module(self, url, out_dir, chosen_stem=None, quality="", container=""):
        import yt_dlp

        last_emit = {"t": 0.0}
        is_audio = quality == "audio"

        def hook(d):
            if d.get("status") == "downloading":
                import time
                now = time.time()
                if now - last_emit["t"] < 0.5:
                    return
                last_emit["t"] = now
                total = d.get("total_bytes") or d.get("total_bytes_estimate") or 0
                done = d.get("downloaded_bytes", 0)
                percent = (done / total * 100.0) if total else 0.0
                speed = d.get("_speed_str", "") or ""
                self.downloadProgress.emit(d.get("info_dict", {}).get("webpage_url", url), percent, speed)

        # If the user picked an exact filename via the save dialog, honor it
        # (yt-dlp still decides the extension based on the merged format,
        # unless a specific container is forced below).
        name_template = (chosen_stem + ".%(ext)s") if chosen_stem else "%(title)s [%(id)s].%(ext)s"

        base_opts = {
            "outtmpl": os.path.join(out_dir, name_template),
            "format": self._format_selector_for(quality),
            "progress_hooks": [hook],
            "noplaylist": True,
            "quiet": True,
            "no_warnings": True,
        }

        if is_audio:
            base_opts["format"] = "bestaudio/best"
        elif container in ("mp4", "webm", "mkv"):
            base_opts["merge_output_format"] = container

        def do_download(opts):
            with yt_dlp.YoutubeDL(opts) as ydl:
                info = ydl.extract_info(url, download=True)
                return ydl.prepare_filename(info)

        try:
            filename = do_download(base_opts)
        except Exception as e:
            # Forcing an incompatible container (e.g. mp4 for a vp9/opus-only
            # stream) can make ffmpeg's remux step fail. Retry once, letting
            # yt-dlp pick a container it knows is compatible (native/mkv)
            # instead of failing the whole download.
            if not is_audio and "merge_output_format" in base_opts:
                fallback_opts = dict(base_opts)
                fallback_opts.pop("merge_output_format", None)
                filename = do_download(fallback_opts)
            else:
                raise

        self.downloadFinished.emit(url, True, filename)

    def _download_with_cli(self, url, out_dir, chosen_stem=None, quality="", container=""):
        import subprocess

        is_audio = quality == "audio"
        name_template = (chosen_stem + ".%(ext)s") if chosen_stem else "%(title)s [%(id)s].%(ext)s"
        out_template = os.path.join(out_dir, name_template)

        def build_cmd(force_container):
            cmd = [
                "yt-dlp",
                "-f", self._format_selector_for(quality),
                "-o", out_template,
            ]
            if not is_audio and force_container:
                cmd += ["--merge-output-format", force_container]
            cmd.append(url)
            return cmd

        force = container if (container in ("mp4", "webm", "mkv") and not is_audio) else None
        result = subprocess.run(build_cmd(force), capture_output=True, text=True)

        if result.returncode != 0 and force:
            # Same fallback as the Python-module path: retry without
            # forcing a container that yt-dlp/ffmpeg couldn't produce.
            result = subprocess.run(build_cmd(None), capture_output=True, text=True)
            force = None

        if result.returncode == 0:
            ext = "m4a" if is_audio else (force or "mp4")
            saved_hint = os.path.join(out_dir, chosen_stem + "." + ext) if chosen_stem else out_dir
            self.downloadFinished.emit(url, True, saved_hint)
        else:
            err = (result.stderr or "yt-dlp failed").strip()
            self.downloadFinished.emit(url, False, err[-300:])

    def _run_info_lookup(self, url):
        try:
            import yt_dlp

            ydl_opts = {
                "quiet": True,
                "no_warnings": True,
                "noplaylist": True,
                "skip_download": True,
            }
            with yt_dlp.YoutubeDL(ydl_opts) as ydl:
                info = ydl.extract_info(url, download=False)

            formats = info.get("formats") or []
            video_formats = [f for f in formats if f.get("vcodec") not in (None, "none") and f.get("height")]
            audio_formats = [f for f in formats if f.get("vcodec") in (None, "none") and f.get("acodec") not in (None, "none")]

            def best_audio():
                if not audio_formats:
                    return None
                return max(audio_formats, key=lambda f: (f.get("abr") or 0))

            def size_of(f):
                if not f:
                    return 0
                return f.get("filesize") or f.get("filesize_approx") or 0

            audio_fmt = best_audio()
            audio_size = size_of(audio_fmt)

            qualities = []
            for tier in QUALITY_TIERS:
                if tier["key"] == "audio":
                    if audio_fmt:
                        qualities.append({
                            "key": "audio",
                            "label": tier["label"],
                            "available": True,
                            "size_bytes": audio_size,
                            "size_label": _fmt_size(audio_size),
                        })
                    continue

                # Best video-only format at or below this tier's height.
                candidates = [f for f in video_formats if f.get("height") and f["height"] <= tier["height"]]
                if not candidates:
                    qualities.append({"key": tier["key"], "label": tier["label"], "available": False,
                                       "size_bytes": 0, "size_label": None})
                    continue
                best_video = max(candidates, key=lambda f: f["height"])
                total = size_of(best_video) + audio_size
                qualities.append({
                    "key": tier["key"],
                    "label": tier["label"],
                    "available": True,
                    "size_bytes": total,
                    "size_label": _fmt_size(total),
                })

            payload = {
                "title": info.get("title") or "",
                "thumbnail": info.get("thumbnail") or "",
                "duration": info.get("duration") or 0,
                "uploader": info.get("uploader") or "",
                "qualities": qualities,
            }
            self.videoInfoReady.emit(url, json.dumps(payload))
        except Exception as e:
            self.videoInfoFailed.emit(url, str(e))
        finally:
            self._info_requests.discard(url)
