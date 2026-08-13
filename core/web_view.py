"""
Enhanced WebView with per-tab profile isolation (like Chrome's process isolation)
Each tab gets its own QWebEngineProfile for memory/security isolation.
"""
import json
import random
from core.browser_qt import (QWebEngineView, QWebEnginePage, QWebEngineProfile,
                        pyqtSignal, QUrl, QWebEngineSettings, QTimer)
from core.browser_resources import (ANTI_FINGERPRINTING_JS, ANTI_TRACKING_JS,
                              TRACKING_DOMAINS, USER_AGENTS, SETTINGS_FILE,
                              DEFAULT_SETTINGS, load_json_file)


class WebPage(QWebEnginePage):
    """Custom WebEnginePage — suppress noisy JS console output"""

    def javaScriptConsoleMessage(self, level, message, lineID, sourceID):
        # Suppress external JS console noise; uncomment to re-enable:
        # print(f"[JS] {message}  ({sourceID}:{lineID})")
        pass


class WebView(QWebEngineView):
    """
    Custom web view with:
    - Per-tab QWebEngineProfile (process isolation)
    - Favicon / new-window signals
    - Security settings support
    - Tab-sleep / wake support
    - Anti-fingerprinting & tracking protection
    - User-Agent spoofing
    - WebRTC IP protection
    - Referrer policy control
    """

    favicon_changed = pyqtSignal(object)          # QIcon
    new_window_requested = pyqtSignal(object, object)  # view, request_type

    # ──────────────────────────────────────────────────────────────
    # Initialisation
    # ──────────────────────────────────────────────────────────────
    def __init__(self, parent=None, profile=None, isolated=True):
        """
        Parameters
        ----------
        profile  : optional shared QWebEngineProfile (e.g. from incognito session)
        isolated : if True and no profile supplied, create a brand-new isolated profile
        """
        if profile:
            # Use provided (shared) profile – e.g. for incognito
            self._owns_profile = False
            self._profile = profile
        elif isolated:
            # ── Per-tab process isolation ──────────────────────────
            # Use a unique name so Qt keeps separate renderer processes
            import uuid
            name = f"cloudar-tab-{uuid.uuid4().hex[:8]}"
            self._profile = QWebEngineProfile(name)
            self._owns_profile = True
        else:
            self._profile = None
            self._owns_profile = False

        # QWebEngineView constructor
        if self._profile:
            super().__init__(self._profile, parent)
        else:
            super().__init__(parent)

        # Attach custom page (same profile)
        active_profile = self._profile or self.page().profile()
        self.setPage(WebPage(active_profile, self))

        # Load settings & apply security
        self.settings_data = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)
        self.apply_security_settings(self.settings_data)
        self._apply_anonymity_settings(self.settings_data)

        self.page().iconChanged.connect(self._on_icon_changed)

        # Tab-sleep tracking
        self._is_sleeping = False

    # ──────────────────────────────────────────────────────────────
    # Profile access
    # ──────────────────────────────────────────────────────────────
    def get_profile(self):
        """Return this tab's QWebEngineProfile."""
        return self._profile or self.page().profile()

    # ──────────────────────────────────────────────────────────────
    # Tab Sleep / Wake
    # ──────────────────────────────────────────────────────────────
    def sleep(self, discard=False):
        """Suspend this tab to save RAM/CPU."""
        try:
            from core.browser_qt import QWebEnginePage
            if discard:
                state = QWebEnginePage.LifecycleState.DiscardedLifecycleState
            else:
                state = QWebEnginePage.LifecycleState.FrozenLifecycleState
            self.page().setLifecycleState(state)
            self._is_sleeping = True
        except Exception:
            pass

    def wake(self):
        """Resume a sleeping tab."""
        try:
            from core.browser_qt import QWebEnginePage
            self.page().setLifecycleState(QWebEnginePage.LifecycleState.ActiveLifecycleState)
            self._is_sleeping = False
        except Exception:
            pass

    @property
    def is_sleeping(self):
        return self._is_sleeping

    # ──────────────────────────────────────────────────────────────
    # Anonymity / Anti-Fingerprinting
    # ──────────────────────────────────────────────────────────────
    def _apply_anonymity_settings(self, settings):
        """Apply anonymity, anti-fingerprinting, and tracking protection settings."""
        profile = self.get_profile()
        if not profile:
            return

        # ── User-Agent spoofing ──
        ua_mode = settings.get("user_agent_mode", "default")
        if ua_mode != "default":
            if ua_mode == "random":
                # Pick a random known UA
                ua = random.choice(list(USER_AGENTS.values()))
            elif ua_mode == "custom":
                ua = settings.get("custom_user_agent", "")
                if not ua:
                    ua = USER_AGENTS.get("chrome", "")
            else:
                ua = USER_AGENTS.get(ua_mode, "")
            if ua:
                try:
                    profile.setHttpUserAgent(ua)
                except Exception:
                    pass

        # ── WebRTC IP protection ──
        # Qt WebEngine doesn't have a direct WebRTC policy API, but we can
        # inject JS to disable non-proxied WebRTC. The profile-level setting
        # is handled via the JS injection below.

        # ── Inject anti-fingerprinting scripts ──
        self._inject_privacy_scripts(settings)

    def _inject_privacy_scripts(self, settings):
        """Inject anti-fingerprinting and anti-tracking scripts into the page."""
        try:
            collection = self.page().scripts()
        except Exception:
            return

        # Remove existing privacy scripts first
        for name in ["_cloudar_anti_fingerprint", "_cloudar_anti_tracking"]:
            try:
                existing = collection.findScript(name)
                if existing:
                    collection.remove(existing)
            except Exception:
                pass

        from core.browser_qt import QWebEngineScript

        # ── Anti-fingerprinting script ──
        if settings.get("block_fingerprinting", True):
            try:
                script = QWebEngineScript()
                script.setName("_cloudar_anti_fingerprint")
                script.setSourceCode(ANTI_FINGERPRINTING_JS)
                script.setInjectionPoint(QWebEngineScript.InjectionPoint.DocumentCreation)
                script.setWorldId(QWebEngineScript.ScriptWorldId.MainWorld)
                script.setRunsOnSubFrames(True)
                collection.insert(script)
            except Exception:
                pass

        # ── Anti-tracking script ──
        if settings.get("block_tracking_scripts", True):
            try:
                tracking_patterns = json.dumps(list(TRACKING_DOMAINS))
                anti_tracking_js = ANTI_TRACKING_JS % tracking_patterns
                script = QWebEngineScript()
                script.setName("_cloudar_anti_tracking")
                script.setSourceCode(anti_tracking_js)
                script.setInjectionPoint(QWebEngineScript.InjectionPoint.DocumentCreation)
                script.setWorldId(QWebEngineScript.ScriptWorldId.MainWorld)
                script.setRunsOnSubFrames(True)
                collection.insert(script)
            except Exception:
                pass

    # ──────────────────────────────────────────────────────────────
    # Security settings
    # ──────────────────────────────────────────────────────────────
    def apply_security_settings(self, settings):
        """Apply security hardening based on settings dict."""
        web_settings = self.settings()
        is_absolute = settings.get("absolute_security", False)

        if is_absolute:
            attributes = {
                'JavascriptEnabled':              False,
                'LocalStorageEnabled':            False,
                'PluginsEnabled':                 False,
                'JavascriptCanOpenWindows':        False,
                'JavascriptCanAccessClipboard':   False,
                'AllowRunningInsecureContent':    False,
                'LocalContentCanAccessRemoteUrls':False,
                'XSSAuditingEnabled':             True,
            }
        else:
            attributes = {
                'JavascriptEnabled':              True,
                'LocalStorageEnabled':            True,
                'PluginsEnabled':                 True,
                'JavascriptCanOpenWindows':       True,
                'JavascriptCanAccessClipboard':   True,
                'AllowRunningInsecureContent':    True,
                'LocalContentCanAccessRemoteUrls':True,
            }

        if settings.get("safe_browsing", True):
            attributes['HyperlinkAuditingEnabled'] = True

        for attr_name, value in attributes.items():
            attr = getattr(QWebEngineSettings.WebAttribute, attr_name, None)
            if attr is not None:
                web_settings.setAttribute(attr, value)

    # ──────────────────────────────────────────────────────────────
    # Internal signals
    # ──────────────────────────────────────────────────────────────
    def _on_icon_changed(self):
        icon = self.page().icon()
        self.favicon_changed.emit(icon)

    def createWindow(self, window_type):
        """Handle target='_blank' or window.open() by opening a new tab."""
        new_view = WebView(profile=self.page().profile(), isolated=False)
        self.new_window_requested.emit(new_view, window_type)
        return new_view

    # ──────────────────────────────────────────────────────────────
    # Cleanup
    # ──────────────────────────────────────────────────────────────
    def cleanup(self):
        """Call when tab is closed to free isolated profile resources.

        Proper cleanup order to avoid QtWebEngine warnings:
        1. Disconnect and delete the page first (releases profile refs)
        2. Clear profile cache
        3. Schedule view and profile deletion
        """
        # Release the page first – it holds a reference to the profile
        try:
            page = self.page()
            if page:
                try:
                    page.iconChanged.disconnect()
                except Exception:
                    pass
                page.deleteLater()
        except Exception:
            pass

        if self._owns_profile and self._profile:
            try:
                # Clear cache & cookies for isolated profile
                self._profile.clearHttpCache()
                self._profile.clearAllVisitedLinks()
            except Exception:
                pass
            # Schedule profile deletion – must happen after page delete
            try:
                self._profile.deleteLater()
            except Exception:
                pass

        # Schedule this view for deletion too
        try:
            self.deleteLater()
        except Exception:
            pass