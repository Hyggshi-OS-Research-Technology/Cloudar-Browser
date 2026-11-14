"""
Bridge for cloudar:// internal pages via QWebChannel.
"""
from core.browser_qt import QObject, pyqtSignal, pyqtSlot


class InternalPagesBridge(QObject):
    bookmarksChanged = pyqtSignal()
    historyChanged = pyqtSignal()
    downloadsChanged = pyqtSignal()
    permissionsChanged = pyqtSignal()
    extensionsChanged = pyqtSignal()
    flagsChanged = pyqtSignal()

    def __init__(self, browser_window):
        super().__init__(browser_window)
        self.browser = browser_window

        dm = getattr(browser_window, "download_manager", None)
        if dm:
            dm.download_started.connect(lambda _: self.downloadsChanged.emit())
            dm.download_progress.connect(lambda *_: self.downloadsChanged.emit())
            dm.download_finished.connect(lambda _: self.downloadsChanged.emit())

        em = getattr(browser_window, "extension_manager", None)
        if em and hasattr(em, "extensions_changed"):
            em.extensions_changed.connect(lambda: self.extensionsChanged.emit())

    # Bookmarks
    @pyqtSlot(result="QVariant")
    def getBookmarks(self):
        bm = self.browser.bookmark_manager
        return {
            "folders": bm.get_folders(),
            "bookmarks": bm.get_bookmarks()
        }

    @pyqtSlot(str, str, str, result=bool)
    def addBookmark(self, title, url, folder=""):
        if not title or not url:
            return False
        ok = self.browser.bookmark_manager.add_bookmark(title, url, folder)
        if ok:
            self.bookmarksChanged.emit()
        return bool(ok)

    @pyqtSlot(str)
    def removeBookmark(self, url):
        if not url:
            return
        self.browser.bookmark_manager.remove_bookmark(url)
        self.bookmarksChanged.emit()

    @pyqtSlot(str, result=bool)
    def addFolder(self, name):
        ok = self.browser.bookmark_manager.add_folder(name)
        if ok:
            self.bookmarksChanged.emit()
        return bool(ok)

    @pyqtSlot(str, bool, result=bool)
    def removeFolder(self, name, removeBookmarks=True):
        ok = self.browser.bookmark_manager.remove_folder(name, removeBookmarks)
        if ok:
            self.bookmarksChanged.emit()
        return bool(ok)

    # History
    @pyqtSlot(int, result="QVariant")
    def getHistory(self, limit=200):
        return self.browser.history_manager.get_history(limit)

    @pyqtSlot(str, result="QVariant")
    def searchHistory(self, query):
        return self.browser.history_manager.search_history(query or "")

    @pyqtSlot()
    def clearHistory(self):
        self.browser.history_manager.clear_history()
        self.historyChanged.emit()

    # Downloads
    @pyqtSlot(result="QVariant")
    def getDownloads(self):
        return self.browser.download_manager.get_downloads()

    @pyqtSlot(str, result=bool)
    def pauseDownload(self, filename):
        ok = bool(self.browser.download_manager.pause_download(filename))
        if ok:
            self.downloadsChanged.emit()
        return ok

    @pyqtSlot(str, result=bool)
    def resumeDownload(self, filename):
        ok = bool(self.browser.download_manager.resume_download(filename))
        if ok:
            self.downloadsChanged.emit()
        return ok

    @pyqtSlot(str)
    def cancelDownload(self, filename):
        self.browser.download_manager.cancel_download(filename)
        self.downloadsChanged.emit()

    @pyqtSlot(str, result=bool)
    def openDownload(self, filename):
        return bool(self.browser.download_manager.open_file(filename))

    @pyqtSlot(str, result=bool)
    def showInFolder(self, filename):
        return bool(self.browser.download_manager.show_in_folder(filename))

    # Permissions
    @pyqtSlot(result="QVariant")
    def getPermissions(self):
        pm = self.browser.permissions_manager
        return pm.get_all()

    @pyqtSlot(str, str, str, result=bool)
    def setPermission(self, origin, permission, value):
        ok = self.browser.permissions_manager.set_permission(origin, permission, value)
        if ok:
            self.permissionsChanged.emit()
        return bool(ok)

    @pyqtSlot()
    def clearPermissions(self):
        self.browser.permissions_manager.clear()
        self.permissionsChanged.emit()

    # Extensions
    @pyqtSlot(result="QVariant")
    def getExtensions(self):
        return self.browser.extension_manager.get_extensions()

    @pyqtSlot(str, bool, result=bool)
    def toggleExtension(self, ext_id, enabled):
        self.browser.extension_manager.toggle_extension(ext_id, enabled)
        if hasattr(self.browser, "reload_extension_scripts"):
            self.browser.reload_extension_scripts()
        self.extensionsChanged.emit()
        return True

    # Flags (cloudar://flags)
    @pyqtSlot(result="QVariant")
    def getFlags(self):
        from core.language import get_text as tr
        items = self.browser.flags_manager.get_all()
        for item in items:
            item["title"] = tr(item.pop("title_key"))
            item["description"] = tr(item.pop("desc_key"))
        return items

    @pyqtSlot(str, bool, result=bool)
    def setFlag(self, flag_id, enabled):
        ok = self.browser.flags_manager.set_flag(flag_id, enabled)
        if ok:
            self.flagsChanged.emit()
        return bool(ok)

    @pyqtSlot(str, result=bool)
    def resetFlag(self, flag_id):
        ok = self.browser.flags_manager.reset_flag(flag_id)
        if ok:
            self.flagsChanged.emit()
        return bool(ok)

    @pyqtSlot()
    def resetAllFlags(self):
        self.browser.flags_manager.reset_all()
        self.flagsChanged.emit()

    @pyqtSlot(result=bool)
    def relaunchBrowser(self):
        if hasattr(self.browser, "relaunch_application"):
            self.browser.relaunch_application()
            return True
        return False
