"""
Torrent Downloader bridge for the browser

The "Torrent Downloader" extension content script detects .torrent links
and magnet: links on pages. For .torrent files, it uses the browser's
native download manager (a simple <a download> click). For magnet: links,
this bridge is called over QWebChannel to handle the actual downloading.

When libtorrent is available, magnet downloads are handled natively in
Python with real progress tracking. Otherwise, the bridge returns false
from isAvailable() and the content script falls back to opening the
magnet in a new tab (a web-based torrent client).

Registration (wired in core/browser_window.py):
    self.torrent_downloader_bridge = TorrentDownloaderBridge(self)
    self.torrent_web_channel.registerObject("torrentDownloader", self.torrent_downloader_bridge)
"""
import json
import os
import re
import shutil
import threading

from core.browser_qt import QObject, pyqtSignal, pyqtSlot
from core.browser_resources import SETTINGS_FILE, DEFAULT_SETTINGS, load_json_file


def _sanitize_filename(name):
    name = (name or "torrent").strip()
    name = re.sub(r'[\\/:*?"<>|]', "_", name)
    return name[:150] if name else "torrent"


def _libtorrent_available():
    """Check if libtorrent is importable."""
    try:
        import libtorrent
        return True
    except ImportError:
        return False


def _parse_magnet_info(magnet_uri):
    """Extract display name (dn) from a magnet URI."""
    name = "Magnet Link"
    try:
        import urllib.parse
        if magnet_uri.startswith("magnet:"):
            parsed = urllib.parse.urlparse(magnet_uri)
            params = urllib.parse.parse_qs(parsed.query)
            if "dn" in params:
                name = params["dn"][0]
    except Exception:
        pass
    return name


class TorrentDownloaderBridge(QObject):
    """Exposed to web pages as `torrentDownloader` via QWebChannel."""

    downloadStarted = pyqtSignal(str)               # url
    downloadProgress = pyqtSignal(str, float, str)  # url, percent, speed_str
    downloadFinished = pyqtSignal(str, bool, str)   # url, success, path_or_error

    def __init__(self, parent=None):
        super().__init__(parent)
        self._active = set()

    # ── Called from JS ──────────────────────────────────────────────

    @pyqtSlot(result=bool)
    def isAvailable(self):
        """Whether libtorrent is installed and can handle magnet downloads."""
        return _libtorrent_available()

    @pyqtSlot(str, str)
    def downloadMagnet(self, magnet_uri, suggested_name=""):
        """Start downloading the content behind a magnet: URI.

        Runs libtorrent in a background thread with real progress
        reporting. The downloaded file(s) are saved to the browser's
        configured download directory.

        Args:
            magnet_uri: The magnet:?xt=urn:btih:... URI to download.
            suggested_name: Optional display name hint from the page.
        """
        if not magnet_uri or not magnet_uri.startswith("magnet:"):
            self.downloadFinished.emit(magnet_uri, False, "Invalid magnet URI")
            return

        if magnet_uri in self._active:
            return

        if not _libtorrent_available():
            self.downloadFinished.emit(magnet_uri, False, "libtorrent not installed")
            return

        self._active.add(magnet_uri)
        threading.Thread(
            target=self._run_magnet_download,
            args=(magnet_uri, suggested_name),
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

    def _run_magnet_download(self, magnet_uri, suggested_name=""):
        """Download a magnet URI using libtorrent.

        This runs in a separate thread to keep the UI responsive.
        Progress is emitted via signals that the QWebChannel forwards
        to the content script.
        """
        import libtorrent as lt

        display_name = suggested_name or _parse_magnet_info(magnet_uri)
        display_name = _sanitize_filename(display_name)
        download_dir = self._get_download_dir()

        self.downloadStarted.emit(magnet_uri)

        try:
            ses = lt.session()
            ses.listen_on(6881, 6891)

            params = {
                "save_path": download_dir,
                "storage_mode": lt.storage_mode_t.storage_mode_sparse,
            }

            handle = lt.add_magnet_uri(ses, magnet_uri, params)

            # Wait for metadata to download (so we know the file names/sizes)
            print(f"[TorrentDownloader] Fetching metadata for: {magnet_uri[:60]}...")
            while not handle.has_metadata():
                status = handle.status()
                if status.state >= lt.torrent_status.downloading:
                    break
                # Give metadata a bit of time; break after 30 seconds timeout
                import time
                timeout = 0
                while timeout < 300:  # 30 seconds
                    time.sleep(0.1)
                    timeout += 1
                    if handle.has_metadata():
                        break
                    if timeout >= 300:
                        raise TimeoutError("Metadata download timed out")

            if not handle.has_metadata():
                self.downloadFinished.emit(magnet_uri, False, "Failed to fetch torrent metadata")
                ses.remove_torrent(handle)
                return

            # Metadata acquired; determine the save path(s)
            torrent_info = handle.torrent_file()
            name = torrent_info.name() or display_name
            final_path = os.path.join(download_dir, name)
            print(f"[TorrentDownloader] Downloading to: {final_path}")

            # Download loop with progress reporting
            last_progress = -1
            last_signal_time = 0.0
            import time

            while handle.status().state != lt.torrent_status.seeding:
                status = handle.status()
                pct = status.progress * 100.0

                # Emit progress at most every 500ms
                now = time.time()
                if now - last_signal_time >= 0.5:
                    last_signal_time = now
                    int_pct = int(pct)
                    if int_pct != last_progress:
                        last_progress = int_pct
                        speed = self._format_speed(status.download_rate)
                        self.downloadProgress.emit(magnet_uri, pct, speed)

                if status.state == lt.torrent_status.checking_files:
                    time.sleep(0.1)
                    continue
                if status.state == lt.torrent_status.downloading_metadata:
                    time.sleep(0.1)
                    continue

                # If paused by user cancel
                if not handle.is_valid():
                    break

                time.sleep(0.2)

            # Download complete or seeding
            self.downloadFinished.emit(magnet_uri, True, final_path)

            # Keep seeding in background for a bit, then remove
            # (We don't want to block the browser - just let the
            # session go out of scope)
            ses.remove_torrent(handle)

        except Exception as e:
            self.downloadFinished.emit(magnet_uri, False, str(e))
        finally:
            self._active.discard(magnet_uri)

    @staticmethod
    def _format_speed(bytes_per_sec):
        if bytes_per_sec <= 0:
            return ""
        units = ["B/s", "KB/s", "MB/s", "GB/s"]
        speed = float(bytes_per_sec)
        i = 0
        while speed >= 1024 and i < len(units) - 1:
            speed /= 1024
            i += 1
        return f"{speed:.1f} {units[i]}"