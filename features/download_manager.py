"""
Enhanced Download Manager for Cloudar Browser™
Features: real-time progress bars, open file, show in folder, cancel, live refresh
"""
import os
from datetime import datetime
from core.browser_qt import (QObject, pyqtSignal, QUrl, QPoint, Qt, QDialog, QVBoxLayout,
                        QWidget, QHBoxLayout, QLabel, QPushButton, QFrame,
                        QScrollArea, QTimer)
from core.language import get_text as tr

# Safe import of QProgressBar / QGraphicsDropShadowEffect / QColor
try:
    from PyQt6.QtWidgets import QProgressBar, QGraphicsDropShadowEffect
    from PyQt6.QtGui import QColor
except ImportError:
    try:
        from PyQt5.QtWidgets import QProgressBar, QGraphicsDropShadowEffect
        from PyQt5.QtGui import QColor
    except ImportError:
        QProgressBar = None
        QGraphicsDropShadowEffect = None
        QColor = None

from core.browser_resources import DOWNLOADS_FILE, load_json_file, save_json_file

# File extensions treated as "playable media" for the popup icon
_MEDIA_EXTS = (".mp4", ".webm", ".mkv", ".avi", ".mov", ".mp3", ".wav", ".m4a", ".flac")


def _fmt_size(num_bytes):
    """Format a byte count as a short human-readable string, e.g. '20,6 MB'."""
    try:
        num_bytes = float(num_bytes)
    except (TypeError, ValueError):
        return "0 B"
    if num_bytes <= 0:
        return "0 B"
    units = ["B", "KB", "MB", "GB", "TB"]
    size = num_bytes
    i = 0
    while size >= 1024 and i < len(units) - 1:
        size /= 1024
        i += 1
    text = f"{size:.0f}" if i == 0 else f"{size:.1f}"
    # Vietnamese (and a few other locales) use a comma as the decimal mark
    try:
        from features.language_manager import LanguageManager
        if LanguageManager.instance().get_current_language() == "vi":
            text = text.replace(".", ",")
    except Exception:
        pass
    return f"{text} {units[i]}"


class DownloadManager(QObject):
    """Manage browser downloads"""

    download_started  = pyqtSignal(str)      # filename
    download_progress = pyqtSignal(str, int) # filename, %
    download_finished = pyqtSignal(str)      # filename

    def __init__(self, parent=None, persist=True):
        super().__init__(parent)
        self.persist = persist
        self.downloads = load_json_file(DOWNLOADS_FILE, default=[]) if persist else []
        self.active_downloads = {}      # filename â†’ QWebEngineDownloadRequest
        self._dialog = None             # Keep reference to open dialog
        self._popup = None              # Keep reference to open toolbar popup

    def handle_download(self, download):
        """Handle a new download request.

        Honors two settings written by the Settings > Downloads page
        (features/settings_backend.py):
          - "download_location": the configured default download folder.
          - "ask_download": if true, prompt for an exact save location for
            every download (like a normal desktop browser's "Ask where to
            save each file" option) instead of silently using the default.

        "force_download_directory" / "forced_download_path" (from the old,
        no-longer-used native Settings dialog) still take priority over
        both when set, as a power-user override that always wins.
        """
        filename = download.downloadFileName()
        download_path = download.downloadDirectory()

        try:
            from core.browser_resources import load_json_file, SETTINGS_FILE, DEFAULT_SETTINGS
            settings = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)

            forced_path = settings.get("forced_download_path", "") if settings.get("force_download_directory", False) else ""

            if forced_path and os.path.isdir(forced_path):
                # Legacy "always use this directory" override wins outright.
                download_path = forced_path
            else:
                configured_dir = settings.get("download_location", "")
                if configured_dir and os.path.isdir(configured_dir):
                    download_path = configured_dir

                if settings.get("ask_download", False):
                    from core.browser_qt import QFileDialog
                    default_path = os.path.join(download_path, filename)
                    chosen_path, _ = QFileDialog.getSaveFileName(
                        None, "Save File", default_path, "Tất cả tệp (*.*)"
                    )
                    if not chosen_path:
                        # User cancelled the save dialog — cancel the
                        # download instead of silently saving anyway.
                        try:
                            download.cancel()
                        except Exception:
                            pass
                        return
                    download_path = os.path.dirname(chosen_path) or download_path
                    filename = os.path.basename(chosen_path)

            if hasattr(download, 'setDownloadDirectory'):
                download.setDownloadDirectory(download_path)
            if hasattr(download, 'setDownloadFileName'):
                download.setDownloadFileName(filename)
        except Exception as e:
            print(f"Error applying download settings: {e}")

        download_info = {
            "filename": filename,
            "path": os.path.join(download_path, filename),
            "url": download.url().toString(),
            "started": datetime.now().isoformat(),
            "completed": False,
            "cancelled": False,
            "progress": 0,
            "paused": False,
        }

        self.active_downloads[filename] = download
        self.downloads.insert(0, download_info)

        download.receivedBytesChanged.connect(
            lambda: self._on_progress(filename, download, download_info)
        )
        download.isFinishedChanged.connect(
            lambda: self._on_finished(filename, download_info, download)
        )

        download.accept()
        self.download_started.emit(filename)

        # Auto-refresh open dialog
        if self._dialog and self._dialog.isVisible():
            self._dialog.refresh()

    def _on_progress(self, filename, download, info):
        total    = download.totalBytes()
        received = download.receivedBytes()
        if total > 0:
            pct = int(received / total * 100)
            info["progress"] = pct
            self.download_progress.emit(filename, pct)
            if self._dialog and self._dialog.isVisible():
                self._dialog.update_progress(filename, pct)
            if self._popup and self._popup.isVisible():
                self._popup.refresh()

    def _on_finished(self, filename, info, download):
        # Check if cancelled
        try:
            from core.browser_qt import QWebEngineDownloadRequest
            if hasattr(download, 'state'):
                from PyQt6.QtWebEngineCore import QWebEngineDownloadRequest as DR
                if download.state() == DR.DownloadState.DownloadCancelled:
                    info["cancelled"] = True
                    info["completed"] = False
                    self.active_downloads.pop(filename, None)
                    self.save()
                    if self._dialog and self._dialog.isVisible():
                        self._dialog.refresh()
                    if self._popup and self._popup.isVisible():
                        self._popup.refresh()
                    return
        except Exception:
            pass

        info["completed"] = True
        info["finished"]  = datetime.now().isoformat()
        info["progress"]  = 100
        info["paused"]    = False
        self.active_downloads.pop(filename, None)
        self.save()
        self.download_finished.emit(filename)

        if self._dialog and self._dialog.isVisible():
            self._dialog.refresh()
        if self._popup and self._popup.isVisible():
            self._popup.refresh()

    def cancel_download(self, filename):
        """Cancel an active download"""
        dl = self.active_downloads.get(filename)
        if dl:
            try:
                dl.cancel()
            except Exception:
                pass

    def pause_download(self, filename):
        """Pause an active download if supported."""
        dl = self.active_downloads.get(filename)
        if not dl:
            return False
        if hasattr(dl, "pause"):
            try:
                dl.pause()
                info = self._find_info(filename)
                if info is not None:
                    info["paused"] = True
                self.save()
                return True
            except Exception:
                return False
        return False

    def resume_download(self, filename):
        """Resume a paused download if supported."""
        dl = self.active_downloads.get(filename)
        if not dl:
            return False
        if hasattr(dl, "resume"):
            try:
                dl.resume()
                info = self._find_info(filename)
                if info is not None:
                    info["paused"] = False
                self.save()
                return True
            except Exception:
                return False
        return False

    def open_file(self, filename):
        info = self._find_info(filename)
        if not info:
            return False
        path = info.get("path", "")
        if path and os.path.exists(path):
            from core.browser_qt import QDesktopServices
            QDesktopServices.openUrl(QUrl.fromLocalFile(path))
            return True
        return False

    def show_in_folder(self, filename):
        info = self._find_info(filename)
        if not info:
            return False
        path = info.get("path", "")
        folder = os.path.dirname(path) if path else os.path.expanduser("~/Downloads")
        from core.browser_qt import QDesktopServices
        QDesktopServices.openUrl(QUrl.fromLocalFile(folder))
        return True

    def get_downloads(self):
        return self.downloads

    def _find_info(self, filename):
        for item in self.downloads:
            if item.get("filename") == filename:
                return item
        return None

    def save(self):
        if self.persist:
            save_json_file(DOWNLOADS_FILE, self.downloads)


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Download Dialog (enhanced)
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
class DownloadItemWidget(QFrame):
    """One row in the downloads list"""

    def __init__(self, info: dict, manager: DownloadManager, parent=None):
        super().__init__(parent)
        self._info    = info
        self._manager = manager
        self._filename = info["filename"]

        self.setObjectName("DownloadItem")
        self.setFrameShape(QFrame.Shape.StyledPanel)

        root = QVBoxLayout(self)
        root.setContentsMargins(10, 8, 10, 8)
        root.setSpacing(4)

        # â”€â”€ Row 1: filename + status buttons â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        top = QHBoxLayout()

        self.name_lbl = QLabel(self._filename)
        self.name_lbl.setStyleSheet("font-weight:600; font-size:13px;")
        top.addWidget(self.name_lbl, 1)

        self.status_lbl = QLabel()
        self.status_lbl.setStyleSheet("font-size:11px; color:#888;")
        top.addWidget(self.status_lbl)

        root.addLayout(top)

        # â”€â”€ Progress bar â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        if QProgressBar:
            self.progress_bar = QProgressBar()
            self.progress_bar.setRange(0, 100)
            self.progress_bar.setFixedHeight(6)
            self.progress_bar.setTextVisible(False)
            self.progress_bar.setStyleSheet("""
                QProgressBar {
                    background: #333;
                    border-radius: 3px;
                    border: none;
                }
                QProgressBar::chunk {
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                        stop:0 #5865f2, stop:1 #7c3aed);
                    border-radius: 3px;
                }
            """)
            root.addWidget(self.progress_bar)
        else:
            self.progress_bar = None

        # â”€â”€ Row 2: action buttons â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        btn_row = QHBoxLayout()
        btn_row.setSpacing(6)

        self.open_btn   = QPushButton("Open")
        self.folder_btn = QPushButton("Show in Folder")
        self.pause_btn  = QPushButton("Pause")
        self.cancel_btn = QPushButton("Cancel")

        for b in (self.open_btn, self.folder_btn, self.pause_btn, self.cancel_btn):
            b.setFixedHeight(24)
            b.setStyleSheet("""
                QPushButton {
                    background: #2a2a2a;
                    border: 1px solid #444;
                    border-radius: 4px;
                    color: #ddd;
                    padding: 0 10px;
                    font-size: 11px;
                }
                QPushButton:hover { background: #3a3a3a; }
                QPushButton:pressed { background: #1a1a1a; }
            """)

        self.open_btn.clicked.connect(self._open_file)
        self.folder_btn.clicked.connect(self._show_in_folder)
        self.pause_btn.clicked.connect(self._toggle_pause)
        self.cancel_btn.clicked.connect(self._cancel)

        btn_row.addWidget(self.open_btn)
        btn_row.addWidget(self.folder_btn)
        btn_row.addWidget(self.pause_btn)
        btn_row.addWidget(self.cancel_btn)
        btn_row.addStretch()
        root.addLayout(btn_row)
        self.refresh()

    def refresh(self):
        info = self._info
        completed = info.get("completed", False)
        cancelled = info.get("cancelled", False)
        progress  = info.get("progress", 0)
        active    = self._filename in self._manager.active_downloads
        paused    = info.get("paused", False)

        if cancelled:
            self.status_lbl.setText("Cancelled")
            self.status_lbl.setStyleSheet("color:#e74c3c; font-size:11px;")
        elif completed:
            self.status_lbl.setText("Completed")
            self.status_lbl.setStyleSheet("color:#2ecc71; font-size:11px;")
        elif active and paused:
            self.status_lbl.setText("Paused")
            self.status_lbl.setStyleSheet("color:#f1c40f; font-size:11px;")
        elif active:
            self.status_lbl.setText(f"Downloading... {progress}%")
            self.status_lbl.setStyleSheet("color:#5865f2; font-size:11px;")
        else:
            self.status_lbl.setText("Interrupted")
            self.status_lbl.setStyleSheet("color:#e67e22; font-size:11px;")

        if self.progress_bar:
            self.progress_bar.setValue(progress if not cancelled else 0)

        # Button visibility
        self.open_btn.setVisible(completed)
        self.folder_btn.setVisible(completed or not active)
        self.pause_btn.setVisible(active)
        self.pause_btn.setText("Resume" if paused else "Pause")
        self.cancel_btn.setVisible(active)

    def update_progress(self, pct: int):
        self._info["progress"] = pct
        self.refresh()

    def _open_file(self):
        from core.browser_qt import QDesktopServices
        path = self._info.get("path", "")
        if path and os.path.exists(path):
            QDesktopServices.openUrl(QUrl.fromLocalFile(path))

    def _show_in_folder(self):
        from core.browser_qt import QDesktopServices
        path = self._info.get("path", "")
        folder = os.path.dirname(path) if path else os.path.expanduser("~/Downloads")
        QDesktopServices.openUrl(QUrl.fromLocalFile(folder))

    def _cancel(self):
        self._manager.cancel_download(self._filename)

    def _toggle_pause(self):
        info = self._info
        if info.get("paused", False):
            self._manager.resume_download(self._filename)
        else:
            self._manager.pause_download(self._filename)
        self.refresh()


class DownloadDialog(QDialog):
    """Enhanced downloads dialog with real-time progress"""

    def __init__(self, download_manager: DownloadManager, parent=None):
        super().__init__(parent)
        self.dm = download_manager
        download_manager._dialog = self
        self.setWindowTitle("Downloads")
        self.setMinimumSize(700, 520)
        self._widgets: dict[str, DownloadItemWidget] = {}
        self._setup_ui()

        # Auto-refresh every 500ms for live progress
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._live_update)
        self._timer.start(500)

    def _setup_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(16, 16, 16, 12)
        root.setSpacing(10)

        # Header
        hdr = QHBoxLayout()
        title = QLabel("Downloads")
        title.setStyleSheet("font-size:20px; font-weight:700; color:#fff;")
        clear_btn = QPushButton("Clear Completed")
        clear_btn.setStyleSheet("""
            QPushButton {
                background:#2a2a2a; border:1px solid #555;
                border-radius:5px; color:#ccc; padding:4px 12px;
            }
            QPushButton:hover { background:#3a3a3a; }
        """)
        clear_btn.clicked.connect(self._clear_completed)
        hdr.addWidget(title)
        hdr.addStretch()
        hdr.addWidget(clear_btn)
        root.addLayout(hdr)

        # Scroll area for items
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("QScrollArea { border:none; background:transparent; }")

        self._list_widget = QWidget()
        self._list_layout = QVBoxLayout(self._list_widget)
        self._list_layout.setContentsMargins(0, 0, 0, 0)
        self._list_layout.setSpacing(6)
        self._list_layout.addStretch()

        scroll.setWidget(self._list_widget)
        root.addWidget(scroll)

        # Populate
        self.refresh()

    def refresh(self):
        """Rebuild full download list."""
        # Clear existing
        while self._list_layout.count() > 1:  # keep the stretch
            item = self._list_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
        self._widgets.clear()

        for info in self.dm.get_downloads():
            w = DownloadItemWidget(info, self.dm, self._list_widget)
            self._widgets[info["filename"]] = w
            self._list_layout.insertWidget(self._list_layout.count() - 1, w)

    def update_progress(self, filename: str, pct: int):
        w = self._widgets.get(filename)
        if w:
            w.update_progress(pct)

    def _live_update(self):
        """Refresh only active download widgets."""
        for fn in list(self.dm.active_downloads.keys()):
            w = self._widgets.get(fn)
            if w:
                w.refresh()

    def _clear_completed(self):
        self.dm.downloads = [d for d in self.dm.downloads
                             if not d.get("completed") and not d.get("cancelled")]
        self.dm.save()
        self.refresh()

    def closeEvent(self, event):
        self._timer.stop()
        self.dm._dialog = None
        super().closeEvent(event)


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Download status popup (Chrome-style bubble under the toolbar icon)
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
class DownloadPopupItem(QFrame):
    """One compact row inside the download status popup."""

    def __init__(self, info: dict, manager: DownloadManager, parent=None):
        super().__init__(parent)
        self._info = info
        self._manager = manager
        self._filename = info.get("filename", "")

        self.setObjectName("DownloadPopupItem")
        self.setStyleSheet("""
            QFrame#DownloadPopupItem { background: transparent; border-radius: 10px; }
            QFrame#DownloadPopupItem:hover { background: rgba(255,255,255,0.06); }
        """)
        self.setCursor(Qt.CursorShape.PointingHandCursor)

        row = QHBoxLayout(self)
        row.setContentsMargins(10, 8, 10, 8)
        row.setSpacing(10)

        ext = os.path.splitext(self._filename)[1].lower()
        icon_char = "\u25b6" if ext in _MEDIA_EXTS else "\u2b07"

        icon_lbl = QLabel(icon_char)
        icon_lbl.setFixedSize(36, 36)
        icon_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        icon_lbl.setStyleSheet("""
            background: rgba(255,255,255,0.08);
            border-radius: 8px;
            font-size: 14px;
            color: #ddd;
        """)
        row.addWidget(icon_lbl)

        text_col = QVBoxLayout()
        text_col.setSpacing(2)

        self.name_lbl = QLabel(self._filename)
        self.name_lbl.setWordWrap(True)
        self.name_lbl.setStyleSheet("color:#fff; font-size:13px; font-weight:600;")
        text_col.addWidget(self.name_lbl)

        self.status_lbl = QLabel()
        self.status_lbl.setStyleSheet("color:#9aa0a6; font-size:11px;")
        text_col.addWidget(self.status_lbl)

        row.addLayout(text_col, 1)
        self.refresh()

    def _size_text(self):
        info = self._info
        path = info.get("path", "")
        try:
            if info.get("completed") and path and os.path.exists(path):
                return _fmt_size(os.path.getsize(path))
        except OSError:
            pass
        total = 0
        dl = self._manager.active_downloads.get(self._filename)
        if dl is not None:
            try:
                total = dl.totalBytes()
            except Exception:
                total = 0
        return _fmt_size(total) if total > 0 else ""

    def refresh(self):
        info = self._info
        active = self._filename in self._manager.active_downloads
        size_text = self._size_text()

        if info.get("cancelled"):
            status = tr("download_popup_cancelled")
        elif info.get("completed"):
            status = tr("download_popup_completed")
        elif active and info.get("paused"):
            status = tr("download_popup_paused")
        elif active:
            status = f"{tr('download_popup_downloading')} {info.get('progress', 0)}%"
        else:
            status = tr("download_popup_completed")

        self.status_lbl.setText(f"{size_text} \u2022 {status}" if size_text else status)

    def mousePressEvent(self, event):
        info = self._info
        path = info.get("path", "")
        if info.get("completed") and path and os.path.exists(path):
            from core.browser_qt import QDesktopServices
            QDesktopServices.openUrl(QUrl.fromLocalFile(path))
        super().mousePressEvent(event)


class DownloadPopup(QFrame):
    """
    Small floating card that appears under the toolbar download button,
    showing the most recent downloads with live progress (Chrome-style).
    """

    MAX_ITEMS = 4

    def __init__(self, download_manager: DownloadManager, parent=None):
        super().__init__(parent, Qt.WindowType.Popup)
        self.dm = download_manager
        self.dm._popup = self
        self.setObjectName("DownloadPopupCard")
        self.setFixedWidth(320)
        self.setStyleSheet("""
            QFrame#DownloadPopupCard {
                background: #2b2b2b;
                border: 1px solid rgba(255,255,255,0.10);
                border-radius: 14px;
            }
        """)

        if QGraphicsDropShadowEffect is not None:
            shadow = QGraphicsDropShadowEffect(self)
            shadow.setBlurRadius(36)
            shadow.setOffset(0, 10)
            shadow.setColor(QColor(0, 0, 0, 170))
            self.setGraphicsEffect(shadow)

        outer = QVBoxLayout(self)
        outer.setContentsMargins(6, 6, 6, 6)
        outer.setSpacing(0)

        self._items_layout = QVBoxLayout()
        self._items_layout.setSpacing(2)
        outer.addLayout(self._items_layout)

        self.empty_lbl = QLabel(tr("download_popup_empty"))
        self.empty_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.empty_lbl.setStyleSheet("color:#9aa0a6; font-size:12px; padding:16px;")
        outer.addWidget(self.empty_lbl)

        outer.addSpacing(2)
        self.show_all_btn = QPushButton(tr("download_popup_show_all"))
        self.show_all_btn.setFlat(True)
        self.show_all_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.show_all_btn.setStyleSheet("""
            QPushButton {
                color: #8ab4f8; background: transparent; border: none;
                padding: 8px; font-size: 12px; text-align: left;
            }
            QPushButton:hover { color: #aecbfa; }
        """)
        self.show_all_btn.clicked.connect(self._show_all)
        outer.addWidget(self.show_all_btn)

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._live_update)
        self._timer.start(500)

        self.refresh()

    def refresh(self):
        while self._items_layout.count():
            item = self._items_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        downloads = self.dm.get_downloads()[: self.MAX_ITEMS]
        self.empty_lbl.setVisible(not downloads)
        for info in downloads:
            self._items_layout.addWidget(DownloadPopupItem(info, self.dm, self))

    def _live_update(self):
        if not self.isVisible():
            return
        if self.dm.active_downloads:
            self.refresh()

    def _show_all(self):
        self.hide()
        parent = self.parent()
        if parent is not None and hasattr(parent, "show_downloads"):
            parent.show_downloads()

    def popup_below(self, anchor_widget):
        """Show this popup anchored just under the given toolbar button."""
        self.refresh()
        anchor_pos = anchor_widget.mapToGlobal(QPoint(0, anchor_widget.height() + 6))
        x = anchor_pos.x() + anchor_widget.width() - self.sizeHint().width()
        self.move(max(x, 0), anchor_pos.y())
        self.show()

    def hideEvent(self, event):
        self._timer.stop()
        if self.dm._popup is self:
            self.dm._popup = None
        super().hideEvent(event)




