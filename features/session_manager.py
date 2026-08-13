"""
Session management for Cloudar Browser™.
"""
import os
from core.browser_qt import QWebEngineProfile
from core.browser_resources import ensure_data_directory, BROWSER_DATA_DIR, SETTINGS_FILE, DEFAULT_SETTINGS, load_json_file
from features.history_manager import HistoryManager, MemoryHistoryManager
from features.download_manager import DownloadManager


class MemoryCookieStore:
    """Tracks cookies in memory and provides explicit cleanup."""

    def __init__(self, profile):
        self.profile = profile
        self.store = profile.cookieStore() if profile else None
        self.cookies = {}

        if self.store is not None:
            if hasattr(self.store, "cookieAdded"):
                self.store.cookieAdded.connect(self._on_cookie_added)
            if hasattr(self.store, "cookieRemoved"):
                self.store.cookieRemoved.connect(self._on_cookie_removed)

    def _cookie_key(self, cookie):
        try:
            name = bytes(cookie.name()).decode("utf-8", errors="ignore")
        except Exception:
            name = str(cookie.name())
        return (cookie.domain(), cookie.path(), name)

    def _on_cookie_added(self, cookie):
        self.cookies[self._cookie_key(cookie)] = cookie

    def _on_cookie_removed(self, cookie):
        self.cookies.pop(self._cookie_key(cookie), None)

    def clear(self):
        self.cookies = {}
        if self.store is not None and hasattr(self.store, "deleteAllCookies"):
            self.store.deleteAllCookies()


class BrowserSession:
    """Normal browsing session backed by disk storage."""

    def __init__(self, data_dir=None):
        self.is_incognito = False
        self.data_dir = data_dir or ensure_data_directory(BROWSER_DATA_DIR)
        self.profile = QWebEngineProfile.defaultProfile()
        self.history_manager = HistoryManager()
        self.download_manager = DownloadManager(persist=True)
        self.cookie_store = None
        self._configure_profile_storage()

    def _configure_profile_storage(self):
        storage_path = os.path.join(self.data_dir, "profile")
        cache_path = os.path.join(self.data_dir, "cache")
        os.makedirs(storage_path, exist_ok=True)
        os.makedirs(cache_path, exist_ok=True)

        if hasattr(self.profile, "setPersistentStoragePath"):
            self.profile.setPersistentStoragePath(storage_path)
        if hasattr(self.profile, "setCachePath"):
            self.profile.setCachePath(cache_path)

        if hasattr(QWebEngineProfile, "HttpCacheType") and hasattr(self.profile, "setHttpCacheType"):
            try:
                self.profile.setHttpCacheType(QWebEngineProfile.HttpCacheType.DiskHttpCache)
            except Exception:
                pass

        if hasattr(QWebEngineProfile, "PersistentCookiesPolicy") and hasattr(self.profile, "setPersistentCookiesPolicy"):
            try:
                self.profile.setPersistentCookiesPolicy(
                    QWebEngineProfile.PersistentCookiesPolicy.AllowPersistentCookies
                )
            except Exception:
                pass

    def cleanup(self):
        """Clean up session data on close.
        
        If auto_delete_cookies_on_exit is enabled, clears all cookies,
        cache, and visited links even for normal sessions.
        """
        # Check if auto-delete is enabled
        settings = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)
        if settings.get("auto_delete_cookies_on_exit", False):
            try:
                self.profile.clearHttpCache()
            except Exception:
                pass
            try:
                self.profile.clearAllVisitedLinks()
            except Exception:
                pass
            # Clear cookies via cookie store
            try:
                store = self.profile.cookieStore()
                if store and hasattr(store, "deleteAllCookies"):
                    store.deleteAllCookies()
            except Exception:
                pass
            # Clear history
            try:
                self.history_manager.clear_history()
            except Exception:
                pass


class IncognitoSession(BrowserSession):
    """Off-the-record session using memory-only storage."""

    def __init__(self, data_dir=None):
        self.is_incognito = True
        self.data_dir = data_dir or ensure_data_directory(BROWSER_DATA_DIR)
        self.profile = QWebEngineProfile()
        self.history_manager = MemoryHistoryManager()
        self.download_manager = DownloadManager(persist=False)
        self.cookie_store = MemoryCookieStore(self.profile)
        self._configure_profile_storage()

    def _configure_profile_storage(self):
        if hasattr(QWebEngineProfile, "HttpCacheType") and hasattr(self.profile, "setHttpCacheType"):
            try:
                self.profile.setHttpCacheType(QWebEngineProfile.HttpCacheType.MemoryHttpCache)
            except Exception:
                pass

        if hasattr(QWebEngineProfile, "PersistentCookiesPolicy") and hasattr(self.profile, "setPersistentCookiesPolicy"):
            try:
                self.profile.setPersistentCookiesPolicy(
                    QWebEngineProfile.PersistentCookiesPolicy.NoPersistentCookies
                )
            except Exception:
                pass

    def cleanup(self):
        """Clean up incognito session data.

        NOTE: All tabs using this profile must be closed BEFORE
        calling this method, otherwise QtWebEngine will emit:
        "Release of profile requested but WebEnginePage still not deleted".
        """
        try:
            self.history_manager.clear_history()
        except Exception:
            pass

        try:
            self.download_manager.downloads = []
            self.download_manager.active_downloads = {}
        except Exception:
            pass

        if self.cookie_store:
            self.cookie_store.clear()

        if hasattr(self.profile, "clearHttpCache"):
            try:
                self.profile.clearHttpCache()
            except Exception:
                pass

        if hasattr(self.profile, "clearAllVisitedLinks"):
            try:
                self.profile.clearAllVisitedLinks()
            except Exception:
                pass

        # Schedule profile deletion – safe now because all tabs
        # referencing this profile have been closed first.
        try:
            self.profile.deleteLater()
        except Exception:
            pass