"""
Main browser window with tabs and navigation
"""
import sys
import ctypes
import os
from core.browser_qt import (QMainWindow, QTabWidget, QTabBar, QToolBar, QLineEdit,
                     QPushButton, QStatusBar, QMenu, QMenuBar, QMessageBox,
                     QWidget, QVBoxLayout, QHBoxLayout, QLabel, QApplication,
                     QUrl, Qt, QSize, QTimer, QAction, QIcon, QKeySequence,
                     QWebEngineProfile, QWebEnginePage, QWebEngineSettings,
                     QWebEngineView, QToolButton, QFrame, QWebChannel, QDockWidget, QCheckBox,
                     QSizePolicy)

from core.web_view import WebView
from features.bookmark_manager import BookmarkManager
from features.history_manager import HistoryDialog
from features.download_manager import DownloadDialog, DownloadPopup
from features.settings_dialog import SettingsDialog
from features.settings_backend import SettingsBackend
from features.find_bar import FindBar
from features.media_control import MediaControlPopup
from core.browser_resources import (ICONS, DEFAULT_SETTINGS, load_json_file, save_json_file,
                             SETTINGS_FILE, env_data_dir, BROWSER_DATA_DIR, ensure_data_directory)
from core.styles import get_stylesheet
from core.url_match import url_matches_any, specificity
from features.extension_manager import ExtensionManager
from features.internal_handler import InternalSchemeHandler, AssetSchemeHandler
from features.session_manager import BrowserSession, IncognitoSession
from features.permissions_manager import PermissionsManager
from features.flags_manager import FlagsManager
from features.internal_pages_bridge import InternalPagesBridge
from features.youtube_downloader import YoutubeDownloaderBridge
from features.torrent_downloader import TorrentDownloaderBridge
from features.ai_sidebar import AISidebar
from features.performance_manager import PerformanceManager
from features.adblock import AdBlockInterceptor
from features.language_manager import LanguageManager
from features.idle_easter_egg import IdleEasterEgg
from core.language import get_text


class BrowserWindow(QMainWindow):
    """Main browser window"""
    
    def __init__(self, incognito=False):
        super().__init__()

        self.is_incognito = bool(incognito)

        # Initialize managers
        ensure_data_directory()
        self.bookmark_manager = BookmarkManager()
        self.settings = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)
        
        # Migrate base64 backgrounds to file-based storage
        self._migrate_background_image()

        # Initialize language manager (must be before UI creation)
        self.lang = LanguageManager.instance()

        # Apply saved language preference (if any)
        saved_lang = self.settings.get("language", "")
        if saved_lang and saved_lang != self.lang.get_current_language():
            self.lang.set_language(saved_lang)

        self.lang.language_changed.connect(self._on_language_changed)

        data_dir = env_data_dir(BROWSER_DATA_DIR)
        self.session = IncognitoSession(data_dir) if self.is_incognito else BrowserSession(data_dir)
        self.history_manager = self.session.history_manager
        self.download_manager = self.session.download_manager
        self.profile = self.session.profile

        self.extension_manager = ExtensionManager(data_dir)
        self.internal_handler = InternalSchemeHandler(self)
        self.performance_manager = PerformanceManager(self)
        self.easter_egg = IdleEasterEgg(self)
        self.permissions_manager = PermissionsManager(persist=not self.is_incognito)
        self.flags_manager = FlagsManager(persist=not self.is_incognito)
        self.settings_backend = SettingsBackend(self)
        self.internal_bridge = InternalPagesBridge(self)
        self.web_channel = QWebChannel()
        self.web_channel.registerObject("settingsBridge", self.settings_backend)
        self.web_channel.registerObject("cloudar", self.settings_backend)
        self.web_channel.registerObject("internalBridge", self.internal_bridge)

        # Separate, restricted channel exposed to regular web pages (e.g.
        # YouTube) so the "Video Downloader" extension can trigger real
        # downloads there. Kept isolated from self.web_channel above so
        # untrusted websites never get access to settingsBridge /
        # internalBridge (bookmarks, history, settings, etc.).
        self.youtube_downloader_bridge = YoutubeDownloaderBridge(self)
        self.youtube_web_channel = QWebChannel()
        self.youtube_web_channel.registerObject("youtubeDownloader", self.youtube_downloader_bridge)

        # Torrent Downloader bridge — also uses a separate restricted
        # channel so the "Torrent Downloader" extension can ask Python
        # to handle magnet: downloads via libtorrent. Exposed on all
        # regular web pages (torrent/magnet links can appear anywhere).
        self.torrent_downloader_bridge = TorrentDownloaderBridge(self)
        self.torrent_web_channel = QWebChannel()
        self.torrent_web_channel.registerObject("torrentDownloader", self.torrent_downloader_bridge)

        # Registry of bridge-name -> QWebChannel. Which sites get which
        # channel is decided entirely by each extension's "matches" /
        # "bridge" declaration in its manifest.json (see
        # _maybe_attach_settings_channel / core/url_match.py), so wiring a
        # brand new bridge-using extension only means adding it to this
        # dict + its manifest, never editing the routing logic below.
        self._extension_channels = {
            "youtubeDownloader": self.youtube_web_channel,
            "torrentDownloader": self.torrent_web_channel,
        }

        self.internal_bridge.bookmarksChanged.connect(self.refresh_bookmark_bar)
        self.internal_bridge.bookmarksChanged.connect(self.update_bookmark_button)

        # Setup download handling
        self.profile.downloadRequested.connect(self.download_manager.handle_download)
        self.download_manager.download_started.connect(self._on_download_started_popup)
        self.download_manager.download_finished.connect(self._on_download_finished_popup)
        self._download_popup = None

        # Register Internal Scheme Handler
        self.profile.installUrlSchemeHandler(b"cloudar", self.internal_handler)
        
        # Register Asset Scheme Handler for user backgrounds
        self.asset_handler = AssetSchemeHandler(self)
        self.profile.installUrlSchemeHandler(b"cloudar-asset", self.asset_handler)

        # Add Extension Scripts to Profile
        self._apply_extension_scripts_to_profile(self.profile)

        # AdBlock interceptor
        self.adblock = AdBlockInterceptor(self)
        adblock_enabled = self.settings.get("adblock_enabled", True)
        self.adblock.set_enabled(adblock_enabled)
        try:
            self.profile.setUrlRequestInterceptor(self.adblock)
        except Exception as e:
            print(f"AdBlock attach failed: {e}")
        self.adblock.blocked_count_changed.connect(self._on_adblock_count_changed)

        # Privacy quick action state tracking
        self._privacy_quick_action_active = False

        # DevTools dock (shared, updated per tab)
        self._devtools_dock = None
        self._devtools_view = None

        # Flag set during closeEvent to allow closing the last tab on shutdown
        self._closing_all = False

        self.setWindowTitle(self._format_window_title())
        self.setMinimumSize(900, 600)

        self.setup_ui()
        self.sync_settings_on_startup()
        self.apply_stylesheet()
        self.apply_privacy_settings()
        self.apply_startup_behavior()

    def _migrate_background_image(self):
        """Migrate base64 background images to file-based storage"""
        try:
            from features.settings_backend import SettingsBackend
            bg = self.settings.get("newtab_background_image", "")
            if bg and bg.startswith("data:"):
                # Use the settings backend migration method
                if hasattr(self, 'settings_backend') and self.settings_backend:
                    new_bg = self.settings_backend._migrate_base64_background(bg)
                    if new_bg != bg:
                        self.settings["newtab_background_image"] = new_bg
                        save_json_file(SETTINGS_FILE, self.settings)
                        print("Migrated background image to cloudar-asset:// protocol")
        except Exception as e:
            print(f"Error migrating background image: {e}")

    def closeEvent(self, event):
        """Handle window close event for session saving

        CRITICAL: Close all tabs BEFORE destroying the shared profile.
        Each tab's page holds a reference to the profile; if we
        delete the profile while pages still exist, QtWebEngine
        emits "Release of profile requested but WebEnginePage
        still not deleted".
        """
        # Always guarantee the event is accepted so the flow completes
        try:
            # Save session first (before tabs are destroyed)
            if not self.is_incognito:
                self.save_current_session()

            # Allow closing the last tab during shutdown
            self._closing_all = True

            # Close all tabs – each will release its page + isolated profile.
            # Use while loop with index 0 so it closes from the front;
            # also force-remove any stubborn tabs.
            for _ in range(self.tabs.count()):
                try:
                    self.close_tab(0)
                except Exception:
                    # If cleanup throws, still forcibly remove the tab
                    widget = self.tabs.widget(0)
                    if widget:
                        try:
                            if hasattr(widget, 'cleanup'):
                                widget.cleanup()
                        except Exception:
                            pass
                    self.tabs.removeTab(0)

            # Clean up Easter egg
            if hasattr(self, 'easter_egg'):
                self.easter_egg.cleanup()

            # Clean up DevTools view if it exists
            if self._devtools_view:
                try:
                    self._devtools_view.page().deleteLater()
                except Exception:
                    pass
                try:
                    self._devtools_view.deleteLater()
                except Exception:
                    pass
                self._devtools_view = None

            # Now it is safe to clean up the shared profile
            if self.is_incognito:
                self.session.cleanup()

        except Exception:
            # Privacy-first: swallow so the window always closes
            pass
        finally:
            event.accept()

    def setup_ui(self):
        """Setup the user interface"""
        # Initialize AI Sidebar (must be done before menu/toolbar)
        self.ai_sidebar = AISidebar(self)
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self.ai_sidebar)
        self.ai_sidebar.hide() # Hidden by default

        # Create menu bar
        self.create_menu_bar()
        
        # Create navigation toolbar
        self.create_navigation_bar()
        self.create_bookmark_bar()
        
        # Main layout container
        main_container = QWidget()
        main_layout = QVBoxLayout(main_container)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        if self.is_incognito:
            self.incognito_banner = self._create_incognito_banner()
            main_layout.addWidget(self.incognito_banner)

        
        # Create tab widget
        from core.tab_widget import TabWidget
        self.tabs = TabWidget()
        self.tabs.setTabsClosable(True)
        self.tabs.setMovable(True)
        self.tabs.setDocumentMode(True)
        self.vertical_tabs_expanded_width = 220
        self.vertical_tabs_collapsed_width = 44
        self.vertical_tabs_hover_expanded = False
        self.tabs.tab_bar.tab_hover_entered.connect(self._on_vertical_tab_bar_hover_entered)
        self.tabs.tab_bar.tab_hover_left.connect(self._on_vertical_tab_bar_hover_left)
        self.apply_vertical_tabs_setting()
        self.tabs.tabCloseRequested.connect(self.close_tab)
        self.tabs.currentChanged.connect(self.current_tab_changed)
        
        # Connect custom tab signals
        self.tabs.new_tab_requested.connect(lambda: self.add_new_tab())
        self.tabs.duplicate_tab_requested.connect(self.duplicate_tab)
        
        # Add new tab button
        self.tabs.setCornerWidget(self.create_new_tab_button(), Qt.Corner.TopRightCorner)
        
        main_layout.addWidget(self.tabs)
        
        # Find Bar (hidden by default)
        self.find_bar = FindBar()
        main_layout.addWidget(self.find_bar)
        
        self.setCentralWidget(main_container)

        # Create status bar
        self.status = QStatusBar()
        self.setStatusBar(self.status)

        # AdBlock status label in status bar
        self._adblock_lbl = QLabel("AdBlock: 0")
        self._adblock_lbl.setObjectName("AdBlockStatus")
        self._adblock_lbl.setToolTip(get_text("adblock_tooltip"))
        self.status.addPermanentWidget(self._adblock_lbl)
    
    def create_menu_bar(self):
        """Create menu bar with translatable text"""
        tr = get_text
        menubar = self.menuBar()
        menubar.setObjectName("ChromeMenuBar")
        try:
            menubar.setNativeMenuBar(False)
        except Exception:
            pass

        # File menu
        self.file_menu = menubar.addMenu(tr("menu_file"))

        self.new_tab_action = QAction(tr("file_new_tab"), self)
        self.new_tab_action.setShortcut(QKeySequence("Ctrl+T"))
        self.new_tab_action.triggered.connect(lambda: self.add_new_tab())
        self.file_menu.addAction(self.new_tab_action)

        self.new_window_action = QAction(tr("file_new_window"), self)
        self.new_window_action.setShortcut(QKeySequence("Ctrl+N"))
        self.new_window_action.triggered.connect(lambda: BrowserWindow(incognito=self.is_incognito).show())
        self.file_menu.addAction(self.new_window_action)

        self.incognito_action = QAction(tr("file_new_incognito"), self)
        self.incognito_action.setShortcut(QKeySequence("Ctrl+Shift+N"))
        self.incognito_action.triggered.connect(self.new_incognito_window)
        self.file_menu.addAction(self.incognito_action)

        self.file_menu.addSeparator()

        self.save_page_action = QAction(tr("file_save_page"), self)
        self.save_page_action.setShortcut(QKeySequence("Ctrl+S"))
        self.save_page_action.triggered.connect(self.save_page)
        self.file_menu.addAction(self.save_page_action)

        self.file_menu.addSeparator()

        self.exit_action = QAction(tr("file_exit"), self)
        self.exit_action.setShortcut(QKeySequence("Ctrl+Q"))
        self.exit_action.triggered.connect(self.close)
        self.file_menu.addAction(self.exit_action)

        # Bookmarks menu
        self.bookmarks_menu = menubar.addMenu(tr("menu_bookmarks"))

        self.add_bookmark_action = QAction(tr("bookmark_add"), self)
        self.add_bookmark_action.setShortcut(QKeySequence("Ctrl+D"))
        self.add_bookmark_action.triggered.connect(self.add_bookmark)
        self.bookmarks_menu.addAction(self.add_bookmark_action)

        self.show_bookmarks_action = QAction(tr("bookmark_show_all"), self)
        self.show_bookmarks_action.triggered.connect(self.show_bookmarks)
        self.bookmarks_menu.addAction(self.show_bookmarks_action)

        self.bookmarks_menu.addSeparator()

        # History menu
        self.history_menu = menubar.addMenu(tr("menu_history"))

        self.show_history_action = QAction(tr("history_show"), self)
        self.show_history_action.setShortcut(QKeySequence("Ctrl+H"))
        self.show_history_action.triggered.connect(self.show_history)
        self.history_menu.addAction(self.show_history_action)

        self.clear_history_action = QAction(tr("history_clear"), self)
        self.clear_history_action.triggered.connect(self.clear_history)
        self.history_menu.addAction(self.clear_history_action)

        # View menu
        self.view_menu = menubar.addMenu(tr("menu_view"))

        self.zoom_in_action = QAction(tr("view_zoom_in"), self)
        self.zoom_in_action.setShortcut(QKeySequence.StandardKey.ZoomIn)
        self.zoom_in_action.triggered.connect(lambda: self.zoom_page(0.1))
        self.view_menu.addAction(self.zoom_in_action)

        self.zoom_out_action = QAction(tr("view_zoom_out"), self)
        self.zoom_out_action.setShortcut(QKeySequence.StandardKey.ZoomOut)
        self.zoom_out_action.triggered.connect(lambda: self.zoom_page(-0.1))
        self.view_menu.addAction(self.zoom_out_action)

        self.reset_zoom_action = QAction(tr("view_zoom_reset"), self)
        self.reset_zoom_action.setShortcut(QKeySequence("Ctrl+0"))
        self.reset_zoom_action.triggered.connect(lambda: self.reset_zoom())
        self.view_menu.addAction(self.reset_zoom_action)

        self.view_menu.addSeparator()

        self.find_action = QAction(tr("view_find"), self)
        self.find_action.setShortcut(QKeySequence.StandardKey.Find)
        self.find_action.triggered.connect(self.toggle_find_bar)
        self.view_menu.addAction(self.find_action)

        self.vertical_tabs_action = QAction("Vertical Tabs", self)
        self.vertical_tabs_action.setCheckable(True)
        self.vertical_tabs_action.setChecked(self.settings.get("vertical_tabs", False))
        self.vertical_tabs_action.triggered.connect(self.toggle_vertical_tabs)
        self.view_menu.addAction(self.vertical_tabs_action)

        self.vertical_tabs_collapse_action = QAction("Collapse Vertical Tabs", self)
        self.vertical_tabs_collapse_action.setCheckable(True)
        self.vertical_tabs_collapse_action.setChecked(self.settings.get("vertical_tabs_collapsed", True))
        self.vertical_tabs_collapse_action.triggered.connect(self.toggle_vertical_tabs_collapsed)
        self.vertical_tabs_collapse_action.setEnabled(self.settings.get("vertical_tabs", False))
        self.view_menu.addAction(self.vertical_tabs_collapse_action)

        self.view_menu.addSeparator()

        self.dev_tools_action = QAction(tr("view_devtools"), self)
        self.dev_tools_action.setShortcut(QKeySequence("F12"))
        self.dev_tools_action.triggered.connect(self.toggle_dev_tools)
        self.view_menu.addAction(self.dev_tools_action)

        self.ai_sidebar_action = QAction(tr("view_ai_sidebar"), self)
        self.ai_sidebar_action.setShortcut(QKeySequence("Ctrl+Shift+A"))
        self.ai_sidebar_action.triggered.connect(self.ai_sidebar.toggle_sidebar)
        self.view_menu.addAction(self.ai_sidebar_action)

        # Tools menu
        self.tools_menu = menubar.addMenu(tr("menu_tools"))

        self.downloads_action = QAction(tr("tools_downloads"), self)
        self.downloads_action.setShortcut(QKeySequence("Ctrl+J"))
        self.downloads_action.triggered.connect(self.show_downloads)
        self.tools_menu.addAction(self.downloads_action)

        self.settings_action = QAction(tr("tools_settings"), self)
        self.settings_action.triggered.connect(self.show_settings)
        self.tools_menu.addAction(self.settings_action)

        self.extensions_action = QAction(tr("tools_extensions"), self)
        self.extensions_action.triggered.connect(lambda: self.add_new_tab(QUrl("cloudar://extensions")))
        self.tools_menu.addAction(self.extensions_action)

        self.permissions_action = QAction(get_text("tools_permissions"), self)
        self.permissions_action.triggered.connect(lambda: self.add_new_tab(QUrl("cloudar://permissions")))
        self.tools_menu.addAction(self.permissions_action)

        self.flags_action = QAction(tr("tools_flags"), self)
        self.flags_action.triggered.connect(lambda: self.add_new_tab(QUrl("cloudar://flags"), title=tr("page_flags")))
        self.tools_menu.addAction(self.flags_action)

        # Language shortcut under Tools opens settings page
        self.tools_menu.addSeparator()
        self.language_action = QAction(tr("tools_language"), self)
        self.language_action.triggered.connect(self._open_language_settings)
        self.tools_menu.addAction(self.language_action)

        # Help menu
        self.help_menu = menubar.addMenu(tr("menu_help"))

        self.about_action = QAction(tr("help_about"), self)
        self.about_action.triggered.connect(lambda: self.add_new_tab(QUrl("cloudar://about")))
        self.help_menu.addAction(self.about_action)

        menubar.hide()

    def _open_language_settings(self):
        """Open the settings page and scroll to the Language section."""
        for i in range(self.tabs.count()):
            widget = self.tabs.widget(i)
            if hasattr(widget, 'url'):
                url_str = widget.url().toString()
                if url_str.startswith("cloudar://settings"):
                    self.tabs.setCurrentIndex(i)
                    widget.setUrl(QUrl("cloudar://settings#language"))
                    return
        
        self.add_new_tab(QUrl("cloudar://settings#language"), title=get_text("settings_tab_title"))

    def _on_language_changed(self, code):
        """
        Called when the language changes. Retranslates all UI text.
        This method updates every translatable string in the window
        without requiring a restart.
        """
        self.retranslate_ui()

    def retranslate_ui(self):
        """
        Retranslate all UI elements after a language change.

        This method walks through every menu, action, toolbar widget,
        and dialog title, replacing the displayed text with the
        current language's translation. It is called automatically
        when the language changes via the Language submenu.
        """
        tr = get_text

        # Menu title
        self.file_menu.setTitle(tr("menu_file"))
        self.bookmarks_menu.setTitle(tr("menu_bookmarks"))
        self.history_menu.setTitle(tr("menu_history"))
        self.view_menu.setTitle(tr("menu_view"))
        self.tools_menu.setTitle(tr("menu_tools"))
        self.help_menu.setTitle(tr("menu_help"))

        # File menu actions
        self.new_tab_action.setText(tr("file_new_tab"))
        self.new_window_action.setText(tr("file_new_window"))
        self.incognito_action.setText(tr("file_new_incognito"))
        self.save_page_action.setText(tr("file_save_page"))
        self.exit_action.setText(tr("file_exit"))

        # Bookmarks menu actions
        self.add_bookmark_action.setText(tr("bookmark_add"))
        self.show_bookmarks_action.setText(tr("bookmark_show_all"))

        # History menu actions
        self.show_history_action.setText(tr("history_show"))
        self.clear_history_action.setText(tr("history_clear"))

        # View menu actions
        self.zoom_in_action.setText(tr("view_zoom_in"))
        self.zoom_out_action.setText(tr("view_zoom_out"))
        self.reset_zoom_action.setText(tr("view_zoom_reset"))
        self.find_action.setText(tr("view_find"))
        self.vertical_tabs_action.setText("Vertical Tabs")
        self.vertical_tabs_collapse_action.setText("Collapse Vertical Tabs")
        self.vertical_tabs_action.setChecked(self.settings.get("vertical_tabs", False))
        self.vertical_tabs_collapse_action.setChecked(self.settings.get("vertical_tabs_collapsed", True))
        self.vertical_tabs_collapse_action.setEnabled(self.settings.get("vertical_tabs", False))
        self.dev_tools_action.setText(tr("view_devtools"))
        self.ai_sidebar_action.setText(tr("view_ai_sidebar"))

        # Tools menu actions      self.downloads_action.setText(tr("tools_downloads"))
        self.settings_action.setText(tr("tools_settings"))
        self.extensions_action.setText(tr("tools_extensions"))
        self.language_action.setText(tr("tools_language"))
        if hasattr(self, "permissions_action"):
            self.permissions_action.setText(get_text("tools_permissions"))
        if hasattr(self, "flags_action"):
            self.flags_action.setText(tr("tools_flags"))

        # Help menu actions
        self.about_action.setText(tr("help_about"))

        # Toolbar widgets
        self.url_bar.setPlaceholderText(tr("url_placeholder"))
        if hasattr(self, 'back_btn'):
            self.back_btn.setToolTip(tr("nav_back"))
        if hasattr(self, 'forward_btn'):
            self.forward_btn.setToolTip(tr("nav_forward"))
        if hasattr(self, 'reload_btn'):
            self.reload_btn.setToolTip(tr("nav_reload"))
        if hasattr(self, 'home_btn'):
            self.home_btn.setToolTip(tr("nav_home"))
        if hasattr(self, 'bookmark_btn'):
            self.bookmark_btn.setToolTip(tr("bookmark_this_page"))
        if hasattr(self, 'translate_btn'):
            self.translate_btn.setToolTip(tr("nav_translate"))
        if hasattr(self, 'split_btn'):
            self.split_btn.setToolTip(tr("nav_split_screen"))
        if hasattr(self, 'ai_btn'):
            self.ai_btn.setToolTip(tr("nav_ai_assistant"))
        if hasattr(self, 'menu_btn'):
            self.menu_btn.setToolTip(tr("nav_menu_tooltip"))
        if hasattr(self, 'new_tab_btn'):
            self.new_tab_btn.setToolTip(tr("file_new_tab"))

        # Incognito banner
        if self.is_incognito and hasattr(self, 'incognito_banner'):
            # Update the banner labels
            for child in self.incognito_banner.findChildren(QLabel):
                if child.objectName() == "IncognitoBannerTitle":
                    child.setText(tr("incognito_title"))
                elif child.objectName() == "IncognitoBannerText":
                    child.setText(tr("incognito_message"))

        # Status bar widgets
        if hasattr(self, '_adblock_lbl'):
            self._adblock_lbl.setToolTip(tr("adblock_tooltip"))

        # AI Sidebar
        if hasattr(self, 'ai_sidebar'):
            self.ai_sidebar.retranslate_ui()

        # Find bar
        if hasattr(self, 'find_bar'):
            self.find_bar.retranslate_ui()

        # Window title
        browser = self.current_browser()
        if browser:
            title = browser.page().title()
            self.setWindowTitle(self._format_window_title(title))
        else:
            self.setWindowTitle(self._format_window_title())
    
    def create_navigation_bar(self):
        """Create a Chrome-inspired navigation bar."""
        tr = get_text
        navbar = QToolBar("Navigation")
        navbar.setObjectName("ChromeNavigationBar")
        navbar.setMovable(False)
        navbar.setFloatable(False)
        navbar.setIconSize(QSize(18, 18))
        self.addToolBar(navbar)

        policy = getattr(QSizePolicy, "Policy", QSizePolicy)
        expanding = policy.Expanding
        fixed = policy.Fixed
        preferred = policy.Preferred

        def with_browser(action):
            def trigger():
                browser = self.current_browser()
                if browser:
                    action(browser)

            return trigger

        def make_button(text, tooltip, slot, object_name):
            btn = QToolButton()
            btn.setObjectName(object_name)
            btn.setText(text)
            btn.setToolTip(tooltip)
            btn.setAutoRaise(True)
            btn.setFixedSize(32, 32)
            btn.clicked.connect(slot)
            return btn

        nav_controls = QWidget()
        nav_controls.setObjectName("ChromeNavControls")
        nav_controls.setSizePolicy(preferred, fixed)
        nav_controls_layout = QHBoxLayout(nav_controls)
        nav_controls_layout.setContentsMargins(0, 0, 0, 0)
        nav_controls_layout.setSpacing(4)

        self.back_btn = make_button(
            ICONS["back"],
            tr("nav_back"),
            with_browser(lambda browser: browser.back()),
            "ChromeNavButton",
        )
        self.forward_btn = make_button(
            ICONS["forward"],
            tr("nav_forward"),
            with_browser(lambda browser: browser.forward()),
            "ChromeNavButton",
        )
        self.reload_btn = make_button(
            ICONS["reload"],
            tr("nav_reload"),
            with_browser(lambda browser: browser.reload()),
            "ChromeNavButton",
        )
        self.home_btn = make_button(
            ICONS["home"],
            tr("nav_home"),
            self.navigate_home,
            "ChromeNavButton",
        )

        nav_controls_layout.addWidget(self.back_btn)
        nav_controls_layout.addWidget(self.forward_btn)
        nav_controls_layout.addWidget(self.reload_btn)
        nav_controls_layout.addWidget(self.home_btn)
        navbar.addWidget(nav_controls)

        self.omnibox = QFrame()
        self.omnibox.setObjectName("ChromeOmnibox")
        self.omnibox.setSizePolicy(expanding, fixed)
        omnibox_layout = QHBoxLayout(self.omnibox)
        omnibox_layout.setContentsMargins(14, 4, 8, 4)
        omnibox_layout.setSpacing(8)

        self.url_status_label = QLabel()
        self.url_status_label.setObjectName("ChromeOmniboxStatus")
        self.url_status_label.setFixedWidth(18)
        omnibox_layout.addWidget(self.url_status_label)

        self.url_bar = QLineEdit()
        self.url_bar.setObjectName("ChromeUrlBar")
        self.url_bar.setPlaceholderText(tr("url_placeholder"))
        self.url_bar.setFrame(False)
        self.url_bar.returnPressed.connect(self.navigate_to_url)
        omnibox_layout.addWidget(self.url_bar, 1)

        self.bookmark_btn = make_button(
            ICONS["bookmark_empty"],
            tr("bookmark_this_page"),
            self.toggle_bookmark,
            "ChromeOmniboxAction",
        )
        self.translate_btn = make_button(
            ICONS["translate"],
            tr("nav_translate"),
            self.translate_page,
            "ChromeOmniboxAction",
        )
        self.translate_btn.setVisible(self.settings.get("translation_enabled", True))

        omnibox_layout.addWidget(self.bookmark_btn)
        omnibox_layout.addWidget(self.translate_btn)
        navbar.addWidget(self.omnibox)

        toolbar_actions = QWidget()
        toolbar_actions.setObjectName("ChromeToolbarActions")
        toolbar_actions.setSizePolicy(preferred, fixed)
        toolbar_actions_layout = QHBoxLayout(toolbar_actions)
        toolbar_actions_layout.setContentsMargins(0, 0, 0, 0)
        toolbar_actions_layout.setSpacing(4)

        self.split_btn = make_button(
            "\u29c9",
            tr("nav_split_screen"),
            self.toggle_split_screen,
            "ChromeToolbarButton",
        )
        self.ai_btn = make_button(
            "\u2726",
            tr("nav_ai_assistant"),
            self.ai_sidebar.toggle_sidebar,
            "ChromeToolbarButton",
        )
        # Privacy quick-action button (hidden by default)
        self.privacy_btn = make_button(
            "\U0001f6e1",
            "Anonymity Mode",
            self.toggle_privacy_quick_action,
            "ChromeToolbarButton",
        )
        self.privacy_btn.setVisible(self.settings.get("privacy_quick_action", False))
        self.privacy_btn.setCheckable(True)
        self.privacy_btn.setChecked(self._privacy_quick_action_active)

        self.download_btn = make_button(
            ICONS["download"],
            tr("tools_downloads"),
            self.toggle_download_popup,
            "ChromeToolbarButton",
        )

        self.menu_btn = make_button(
            ICONS["menu"],
            tr("nav_menu_tooltip"),
            self.show_menu,
            "ChromeMenuButton",
        )

        toolbar_actions_layout.addWidget(self.split_btn)
        toolbar_actions_layout.addWidget(self.ai_btn)
        toolbar_actions_layout.addWidget(self.privacy_btn)
        toolbar_actions_layout.addWidget(self.download_btn)
        toolbar_actions_layout.addWidget(self.menu_btn)
        navbar.addWidget(toolbar_actions)

        self.focus_location_action = QAction(self)
        self.focus_location_action.setShortcut(QKeySequence("Ctrl+L"))
        self.focus_location_action.triggered.connect(self.focus_url_bar)
        self.addAction(self.focus_location_action)

        self._update_url_status_label(QUrl())
        self.update_navigation_state()

    def create_bookmark_bar(self):
        """Create a bookmark bar under the address bar."""
        self.addToolBarBreak()
        bar = QToolBar("Bookmarks")
        bar.setMovable(False)
        bar.setFloatable(False)
        bar.setIconSize(QSize(16, 16))
        bar.setObjectName("BookmarksBar")
        self.bookmark_bar = bar
        self.addToolBar(bar)
        self.refresh_bookmark_bar()

    def refresh_bookmark_bar(self):
        """Rebuild the bookmark bar from stored bookmarks."""
        if not hasattr(self, "bookmark_bar"):
            return
        bar = self.bookmark_bar
        bar.clear()

        bookmarks = self.bookmark_manager.get_bookmarks()
        folders = self.bookmark_manager.get_folders()
        show_bar = self.settings.get("show_bookmark_bar", False)

        if not show_bar:
            bar.setVisible(False)
            return

        # Folders with dropdown menus
        for folder in folders:
            folder_items = [b for b in bookmarks if b.get("folder", "") == folder]
            if not folder_items:
                continue
            menu = QMenu(self)
            for b in folder_items:
                title = b.get("title") or b.get("url")
                action = menu.addAction(title)
                action.setToolTip(b.get("url", ""))
                action.triggered.connect(lambda _, u=b.get("url", ""): self.current_browser().setUrl(QUrl(u)))

            btn = QToolButton()
            btn.setObjectName("BookmarkFolderButton")
            btn.setText(folder)
            btn.setToolTip(folder)
            try:
                btn.setPopupMode(QToolButton.ToolButtonPopupMode.InstantPopup)
            except Exception:
                try:
                    btn.setPopupMode(QToolButton.InstantPopup)
                except Exception:
                    pass
            btn.setMenu(menu)
            bar.addWidget(btn)

        # Root bookmarks
        root_items = [b for b in bookmarks if not b.get("folder")]
        for b in root_items:
            title = b.get("title") or b.get("url")
            action = bar.addAction(title)
            action.setToolTip(b.get("url", ""))
            action.triggered.connect(lambda _, u=b.get("url", ""): self.current_browser().setUrl(QUrl(u)))
            widget = bar.widgetForAction(action)
            if widget:
                widget.setObjectName("BookmarkBarItem")

        if not bookmarks:
            label = QLabel(get_text("bookmark_bar_empty"))
            label.setObjectName("BookmarkBarHint")
            bar.addWidget(label)

        bar.setVisible(True)
    
    def create_new_tab_button(self):
        """Create new tab button"""
        self.new_tab_btn = QToolButton()
        self.new_tab_btn.setObjectName("ChromeNewTabButton")
        self.new_tab_btn.setText(ICONS["new_tab"])
        self.new_tab_btn.setToolTip(get_text("file_new_tab"))
        self.new_tab_btn.setAutoRaise(True)
        self.new_tab_btn.setFixedSize(32, 32)
        self.new_tab_btn.clicked.connect(lambda: self.add_new_tab())
        return self.new_tab_btn

    def _create_incognito_banner(self):
        """Create a banner explaining incognito mode."""
        tr = get_text
        banner = QFrame()
        banner.setObjectName("IncognitoBanner")

        layout = QHBoxLayout(banner)
        layout.setContentsMargins(12, 6, 12, 6)
        layout.setSpacing(12)

        title = QLabel(tr("incognito_title"))
        title.setObjectName("IncognitoBannerTitle")

        text = QLabel(tr("incognito_message"))
        text.setObjectName("IncognitoBannerText")
        text.setWordWrap(True)

        layout.addWidget(title)
        layout.addWidget(text, 1)
        layout.addStretch(0)
        return banner

    def _maybe_attach_settings_channel(self, browser, url):
        if url.scheme() == "cloudar" and url.host() in (
            "settings", "newtab", "bookmarks", "history", "downloads", "permissions", "extensions"
        ):
            try:
                browser.page().setWebChannel(self.web_channel)
            except Exception:
                pass
            return

        # Regular web pages: each enabled extension declares in its
        # manifest.json which sites it needs its bridge on ("matches")
        # and which bridge that is ("bridge") - e.g. Video Downloader
        # asks for "youtubeDownloader" on youtube.com/youtu.be because
        # YouTube's <video> uses MSE/blob URLs a plain <a download> can't
        # save, while Torrent Downloader asks for "torrentDownloader" on
        # every site since magnet/torrent links can appear anywhere.
        # Adding a new bridge-using extension is then just a manifest +
        # self._extension_channels entry, never a code change here. All
        # of these channels are restricted: they only expose
        # download-related methods, never settingsBridge / internalBridge.
        channel = self._resolve_extension_channel(url)
        if channel is None:
            return
        try:
            browser.page().setWebChannel(channel)
        except Exception:
            pass

    def _resolve_extension_channel(self, url):
        """Pick the QWebChannel for `url` based on enabled extensions'
        manifest "matches"/"bridge" declarations. If more than one
        extension matches, the most specific pattern wins (so a targeted
        match like *.youtube.com beats a catch-all like *://*/*)."""
        best_channel = None
        best_score = None
        for ext in self.extension_manager.get_extensions():
            if not ext.get("enabled"):
                continue
            bridge_name = ext.get("bridge")
            patterns = ext.get("matches")
            if not bridge_name or not patterns:
                continue
            channel = self._extension_channels.get(bridge_name)
            if channel is None:
                continue
            matching = [p for p in patterns if url_matches_any(url, [p])]
            if not matching:
                continue
            score = max(specificity(p) for p in matching)
            if best_score is None or score > best_score:
                best_score = score
                best_channel = channel
        return best_channel

    def _connect_permissions(self, browser):
        """Connect permission request signals for a web view."""
        try:
            page = browser.page()
            if hasattr(page, "featurePermissionRequested"):
                page.featurePermissionRequested.connect(
                    lambda url, feature, b=browser: self._on_feature_permission_requested(b, url, feature)
                )
        except Exception:
            pass

    def _apply_feature_permission(self, browser, url, feature, allow: bool):
        try:
            policy = QWebEnginePage.PermissionPolicy.PermissionGrantedByUser if allow else \
                QWebEnginePage.PermissionPolicy.PermissionDeniedByUser
            browser.page().setFeaturePermission(url, feature, policy)
        except Exception:
            pass

    def _on_feature_permission_requested(self, browser, url, feature):
        """Handle feature permission requests with persisted decisions."""
        origin = f"{url.scheme()}://{url.host()}"
        if url.port() != -1:
            origin = f"{origin}:{url.port()}"

        pm = self.permissions_manager

        geo = getattr(QWebEnginePage.Feature, "Geolocation", None)
        aud = getattr(QWebEnginePage.Feature, "MediaAudioCapture", None)
        vid = getattr(QWebEnginePage.Feature, "MediaVideoCapture", None)
        av  = getattr(QWebEnginePage.Feature, "MediaAudioVideoCapture", None)
        notif = getattr(QWebEnginePage.Feature, "Notifications", None)

        if feature == av:
            cam = pm.get_permission(origin, "camera")
            mic = pm.get_permission(origin, "microphone")
            if cam in ("allow", "deny") and mic in ("allow", "deny"):
                self._apply_feature_permission(browser, url, feature, cam == "allow" and mic == "allow")
                return
            perm_key = "camera"
            perm_label = "camera and microphone"
            perms_to_store = ["camera", "microphone"]
        elif feature == geo:
            perm_key = "location"
            perm_label = "location"
            perms_to_store = [perm_key]
        elif feature == aud:
            perm_key = "microphone"
            perm_label = "microphone"
            perms_to_store = [perm_key]
        elif feature == vid:
            perm_key = "camera"
            perm_label = "camera"
            perms_to_store = [perm_key]
        elif feature == notif:
            perm_key = "notifications"
            perm_label = "notifications"
            perms_to_store = [perm_key]
        else:
            return

        decision = pm.get_permission(origin, perm_key)
        if decision in ("allow", "deny"):
            self._apply_feature_permission(browser, url, feature, decision == "allow")
            return

        box = QMessageBox(self)
        box.setWindowTitle(get_text("permission_request_title"))
        box.setText(get_text("permission_request_message").format(origin=origin, permission=perm_label))
        remember = QCheckBox(get_text("permission_remember"))
        box.setCheckBox(remember)
        allow_btn = box.addButton(get_text("permission_allow"), QMessageBox.ButtonRole.AcceptRole)
        box.addButton(get_text("permission_deny"), QMessageBox.ButtonRole.RejectRole)
        box.exec()

        allow = box.clickedButton() == allow_btn
        self._apply_feature_permission(browser, url, feature, allow)

        if remember.isChecked():
            for key in perms_to_store:
                pm.set_permission(origin, key, "allow" if allow else "deny")
            if hasattr(self, "internal_bridge"):
                self.internal_bridge.permissionsChanged.emit()

    def add_new_tab(self, url=None, title="New Tab", background=False, browser=None):
        """Add a new tab with an isolated profile (process isolation)"""
        if url is None and not browser:
            url = QUrl(self._get_homepage_url())
        elif url is None:
            url = QUrl("cloudar://newtab")

        # Per-tab process isolation
        # Incognito uses the shared incognito profile;
        # normal tabs each get their own isolated profile.
        if not browser:
            if self.is_incognito:
                browser = WebView(profile=self.profile, isolated=False)
            else:
                browser = WebView(isolated=True)   # fresh profile per tab
            
            # Install scheme handlers on tab's profile (whether isolated or shared)
            tab_profile = browser.get_profile()
            tab_profile.downloadRequested.connect(
                self.download_manager.handle_download)
            self._apply_extension_scripts_to_profile(tab_profile)
            try:
                if not self.is_incognito:
                    tab_profile.setUrlRequestInterceptor(self.adblock)
            except Exception:
                pass
            try:
                if hasattr(self, 'internal_handler'):
                    tab_profile.installUrlSchemeHandler(
                        b"cloudar", self.internal_handler)
                if hasattr(self, 'asset_handler'):
                    tab_profile.installUrlSchemeHandler(
                        b"cloudar-asset", self.asset_handler)
            except Exception:
                pass
        
        browser.setUrl(url)
        self._maybe_attach_settings_channel(browser, url)

        # Connect signals
        browser.urlChanged.connect(lambda url, b=browser: self.update_url_bar(url, b))
        browser.urlChanged.connect(lambda url, b=browser: self._maybe_attach_settings_channel(b, url))
        browser.loadFinished.connect(lambda _, b=browser: self.update_title(b))
        browser.loadProgress.connect(self.update_progress)
        browser.favicon_changed.connect(lambda icon, b=browser: self.update_favicon(icon, b))
        browser.new_window_requested.connect(self.handle_new_window_request)
        self._connect_permissions(browser)

        # Register with performance manager for sleep tracking
        self.performance_manager.on_tab_created(browser)

        # Add tab
        i = self.tabs.addTab(browser, title)

        if not background:
            self.tabs.setCurrentIndex(i)

        return browser

    def handle_new_window_request(self, new_view, request_type):
        """Handle signal to open a new tab/window from a page"""
        self.add_new_tab(browser=new_view, title="Loading...", background=False)

    def new_incognito_window(self):
        """Open a new specific incognito window"""
        incognito_window = BrowserWindow(incognito=True)
        incognito_window.show()

    def close_tab(self, i):
        """Close a tab and clean up its isolated profile"""
        if self.tabs.count() < 2 and not self._closing_all:
            return

        browser = self.tabs.widget(i)
        # Notify performance manager
        if browser:
            self.performance_manager.on_tab_closed(browser)
        # Do NOT fetch URL from browser after cleanup - save before
        if browser and hasattr(browser, 'url'):
            try:
                _ = browser.url()  # drain any pending signals
            except Exception:
                pass
        # Clean up isolated profile (deletes page, then profile, then view)
        if browser and hasattr(browser, 'cleanup'):
            browser.cleanup()

        self.tabs.removeTab(i)

    def duplicate_tab(self, i):
        """Duplicate the tab at index i"""
        if i >= 0:
            widget = self.tabs.widget(i)
            if hasattr(widget, 'url'):
                url = widget.url()
                self.add_new_tab(url, title="Duplicating...", background=False)
    
    def current_tab_changed(self, i):
        """Handle tab change wake sleeping tab + update perf manager"""
        if i >= 0:
            browser = self.current_browser()
            if browser:
                self.update_url_bar(browser.url(), browser)
                self.update_bookmark_button()
                self.find_bar.set_web_view(browser)

                # Update window title
                title = browser.page().title()
                self.setWindowTitle(self._format_window_title(title))

                # Notify performance manager (wakes sleeping tab)
                self.performance_manager.on_tab_activated(browser)

                # Update DevTools if docked panel is open
                if self._devtools_dock and self._devtools_dock.isVisible():
                    self._attach_devtools_to(browser)

    def _format_window_title(self, title=None):
        """Format window title with optional incognito indicator."""
        if title:
            if self.is_incognito:
                return f"{title} - Incognito - Cloudar Browser™"
            return f"{title} - Cloudar Browser™"
        return "Incognito - Cloudar Browser™" if self.is_incognito else "Cloudar Browser™"

    def _get_homepage_url(self):
        """Return the configured homepage or the default new tab page."""
        homepage = self.settings.get("home_page", "").strip()
        return homepage if homepage else "cloudar://newtab"

    def sync_settings_on_startup(self):
        """Load config/settings.json and apply to browser settings."""
        try:
            if hasattr(self, "settings_backend"):
                self.settings_backend.apply_loaded_settings()
                self.settings = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)
        except Exception as e:
            print(f"Error syncing settings on startup: {e}")

    def focus_url_bar(self):
        """Focus and select the address bar contents."""
        self.url_bar.setFocus()
        self.url_bar.selectAll()

    def update_navigation_state(self, browser=None):
        """Refresh back/forward button enabled state for the active tab."""
        active_browser = self.current_browser()
        browser = browser or active_browser
        is_ready = bool(browser)

        if browser and active_browser and browser != active_browser:
            return

        history = browser.history() if is_ready and hasattr(browser, "history") else None
        can_go_back = is_ready and (
            history.canGoBack() if history is not None else hasattr(browser, "back")
        )
        can_go_forward = is_ready and (
            history.canGoForward() if history is not None else hasattr(browser, "forward")
        )

        if hasattr(self, "back_btn"):
            self.back_btn.setEnabled(can_go_back)
        if hasattr(self, "forward_btn"):
            self.forward_btn.setEnabled(can_go_forward)
        if hasattr(self, "reload_btn"):
            self.reload_btn.setEnabled(is_ready and hasattr(browser, "reload"))
        if hasattr(self, "home_btn"):
            self.home_btn.setEnabled(is_ready and hasattr(browser, "setUrl"))

    def _update_url_status_label(self, url):
        """Show a compact status hint in the omnibox."""
        label = getattr(self, "url_status_label", None)
        if label is None:
            return

        if not url or url.isEmpty():
            label.setText("\u2315")
            label.setToolTip("")
            return

        scheme = (url.scheme() or "").lower()
        if scheme == "https":
            label.setText(ICONS["lock"])
            label.setToolTip("HTTPS")
        elif scheme == "http":
            label.setText("\u26a0")
            label.setToolTip("HTTP")
        elif scheme == "cloudar":
            label.setText(ICONS["home"])
            label.setToolTip("Cloudar")
        else:
            label.setText("\u2315")
            label.setToolTip(url.host() or url.toString())

    def current_browser(self):
        """Get current browser widget"""
        tabs = getattr(self, "tabs", None)
        return tabs.currentWidget() if tabs is not None else None

    def navigate_to_url(self):
        """Navigate to URL from address bar"""
        url_str = self.url_bar.text().strip()

        if not url_str:
            return

        # Check if it's a local file path
        if url_str.startswith("/") or url_str.startswith("~"):
            # Convert local path to file:// URL
            expanded = os.path.expanduser(url_str)
            qurl = QUrl.fromLocalFile(expanded)
        # Check if it's a URL with scheme
        elif "://" in url_str:
            qurl = QUrl(url_str)
        elif "." in url_str and " " not in url_str:
            qurl = QUrl("https://" + url_str)
        else:
            search_engine = self.settings.get("search_engine", "https://www.google.com/search?q={}")
            qurl = QUrl(search_engine.format(url_str.replace(" ", "+")))

        # Absolute Security: Enforce HTTPS (skip for local files)
        if self.settings.get("absolute_security", False) and qurl.scheme() == "http":
            qurl.setScheme("https")

        browser = self.current_browser()
        if browser:
            browser.setUrl(qurl)

    def navigate_home(self):
        """Navigate to home page"""
        home_url = self._get_homepage_url()
        browser = self.current_browser()
        if browser:
            browser.setUrl(QUrl(home_url))

    def update_url_bar(self, url, browser=None):
        """Update URL bar"""
        if browser != self.current_browser():
            return

        self.url_bar.setText(url.toString())
        self.url_bar.setCursorPosition(0)
        self._update_url_status_label(url)
        self.update_navigation_state(browser)
        self.update_bookmark_button()

    def update_title(self, browser):
        """Update tab title"""
        title = browser.page().title()
        i = self.tabs.indexOf(browser)

        if i >= 0:
            self.tabs.setTabText(i, title[:30] + "..." if len(title) > 30 else title)

        if browser != self.current_browser():
            return

        self.setWindowTitle(self._format_window_title(title))

        # Add to history
        if not self.is_incognito:
            self.history_manager.add_history(title, browser.url().toString())
            if hasattr(self, "internal_bridge"):
                self.internal_bridge.historyChanged.emit()

    def update_favicon(self, icon, browser):
        """Update tab favicon"""
        i = self.tabs.indexOf(browser)
        if i >= 0:
            self.tabs.setTabIcon(i, icon)
    
    def update_progress(self, progress):
        """Update loading progress"""
        if progress < 100:
            self.status.showMessage(f"Loading... {progress}%")
        else:
            self.status.clearMessage()
    
    def add_bookmark(self):
        """Add current page to bookmarks"""
        tr = get_text
        browser = self.current_browser()
        if browser:
            title = browser.page().title()
            url = browser.url().toString()

            if self.bookmark_manager.is_bookmarked(url):
                QMessageBox.information(self, tr("bookmark_dialog_title"), tr("bookmark_already"))
            else:
                self.bookmark_manager.add_bookmark(title, url)
                self.update_bookmark_button()
                self.refresh_bookmark_bar()
                if hasattr(self, "internal_bridge"):
                    self.internal_bridge.bookmarksChanged.emit()
                self.status.showMessage(tr("bookmark_added"), 2000)
    
    def toggle_bookmark(self):
        """Toggle bookmark for current page"""
        tr = get_text
        browser = self.current_browser()
        if browser:
            url = browser.url().toString()

            if self.bookmark_manager.is_bookmarked(url):
                self.bookmark_manager.remove_bookmark(url)
                self.status.showMessage(tr("bookmark_removed"), 2000)
            else:
                title = browser.page().title()
                self.bookmark_manager.add_bookmark(title, url)
                self.status.showMessage(tr("bookmark_added"), 2000)

            self.update_bookmark_button()
            self.refresh_bookmark_bar()
            if hasattr(self, "internal_bridge"):
                self.internal_bridge.bookmarksChanged.emit()
    
    def update_bookmark_button(self):
        """Update bookmark button icon"""
        browser = self.current_browser()
        if browser:
            url = browser.url().toString()
            if self.bookmark_manager.is_bookmarked(url):
                self.bookmark_btn.setText(ICONS["bookmark"])
            else:
                self.bookmark_btn.setText(ICONS["bookmark_empty"])
    
    def show_bookmarks(self):
        """Open bookmarks manager page."""
        self.add_new_tab(QUrl("cloudar://bookmarks"), title=get_text("page_bookmarks"), background=False)
    
    def navigate_to_bookmark(self, bookmark):
        """Navigate to a bookmark"""
        self.current_browser().setUrl(QUrl(bookmark["url"]))
    
    def show_history(self):
        """Open history page."""
        self.add_new_tab(QUrl("cloudar://history"), title=get_text("page_history"), background=False)
    
    def clear_history(self):
        """Clear browsing history (shortcut access)"""
        tr = get_text
        reply = QMessageBox.question(
            self, tr("history_clear"),
            tr("history_clear_confirm"),
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            self.history_manager.clear_history()
            self.status.showMessage(tr("history_cleared"), 2000)
            if hasattr(self, "internal_bridge"):
                self.internal_bridge.historyChanged.emit()
    
    def show_downloads(self):
        """Open downloads page."""
        self.add_new_tab(QUrl("cloudar://downloads"), title=get_text("page_downloads"), background=False)

    def _ensure_download_popup(self):
        if self._download_popup is None or not self._download_popup.parent():
            self._download_popup = DownloadPopup(self.download_manager, self)
        return self._download_popup

    def toggle_download_popup(self):
        """Toggle the Chrome-style download status bubble under the toolbar button."""
        if not self.flags_manager.is_enabled("download-bubble"):
            self.show_downloads()
            return
        popup = self._ensure_download_popup()
        if popup.isVisible():
            popup.hide()
        else:
            popup.popup_below(self.download_btn)

    def _on_download_started_popup(self, filename):
        """Auto-show the status popup whenever a new download begins."""
        if not self.flags_manager.is_enabled("download-bubble"):
            return
        popup = self._ensure_download_popup()
        popup.popup_below(self.download_btn)

    def _on_download_finished_popup(self, filename):
        """Refresh the status popup so completed downloads show 'Done'."""
        if self._download_popup is not None and self._download_popup.isVisible():
            self._download_popup.refresh()

    def show_settings(self):
        """Show settings page"""
        self.add_new_tab(QUrl("cloudar://settings"), title=get_text("settings_tab_title"), background=False)

    def relaunch_application(self):
        """Restart the whole browser process (used by cloudar://flags 'Relaunch')."""
        import sys
        python = sys.executable
        script = os.path.abspath(sys.argv[0])
        args = sys.argv[1:]
        QApplication.instance().quit()
        os.execv(python, [python, script] + args)

    def on_settings_changed(self, settings):
        """Handle settings changes"""
        tr = get_text
        # Check for special actions
        if "action" in settings:
            if settings["action"] == "clear_data":
                self.history_manager.clear_history()
                self.profile.clearHttpCache()
                self.profile.clearAllVisitedLinks()
                self.status.showMessage(tr("data_cleared"), 3000)
            return

        self.settings = settings
        self.status.showMessage(tr("settings_saved"), 2000)
        
        # Apply immediate settings
        # Startup behavior is handled on restart
        # Download location is pulled from settings when needed
        # Do Not Track needs to be applied to profile
        if self.settings.get("do_not_track", False):
            # There isn't a direct "Do Not Track" header setting in standard WebEngine API
            # effectively without injecting headers.
            # We can set a user agent or similar, but for now we'll skip complex header injection.
            pass
            
        # Default zoom applier
        # We need to parse "100%" -> 1.0
        zoom_text = self.settings.get("default_zoom", "100%")
        try:
            factor = int(zoom_text.replace("%", "")) / 100.0
            if self.tabs.currentWidget():
                self.tabs.currentWidget().setZoomFactor(factor)
        except Exception as e:
            print(f"Error applying zoom: {e}")
            
        # Update Translate Button Visibility
        if hasattr(self, 'translate_btn'):
            self.translate_btn.setVisible(self.settings.get("translation_enabled", True))

        if hasattr(self, "bookmark_bar"):
            self.refresh_bookmark_bar()

        if hasattr(self, "tabs"):
            self.apply_vertical_tabs_setting()
            
        # Update Theme
        self.apply_stylesheet()
        
        # Update Security on all tabs
        for i in range(self.tabs.count()):
            browser = self.tabs.widget(i)
            if hasattr(browser, 'apply_security_settings'):
                browser.apply_security_settings(self.settings)
            if hasattr(browser, '_apply_anonymity_settings'):
                browser._apply_anonymity_settings(self.settings)

        self.apply_privacy_settings()

    def apply_privacy_settings(self):
        """Apply global privacy settings to the profile"""
        profile = self.profile

        if self.is_incognito:
            if hasattr(QWebEngineProfile, "PersistentCookiesPolicy") and hasattr(profile, "setPersistentCookiesPolicy"):
                profile.setPersistentCookiesPolicy(
                    QWebEngineProfile.PersistentCookiesPolicy.NoPersistentCookies
                )
            return

        # Third-party cookies policy
        # 0: Always, 1: Never, 2: Only from sites navigated to
        if self.settings.get("third_party_cookies", "block") == "block":
            profile.setPersistentCookiesPolicy(QWebEngineProfile.PersistentCookiesPolicy.NoPersistentCookies)
            # Note: For true third-party blocking, more complex filtering is needed.
            # But this is a good start for "Privacy"
        else:
            profile.setPersistentCookiesPolicy(QWebEngineProfile.PersistentCookiesPolicy.AllowPersistentCookies)

    def _apply_extension_scripts_to_profile(self, profile):
        """Attach extension scripts to a given profile."""
        if not profile:
            return
        try:
            collection = profile.scripts()
        except Exception:
            return

        # Remove existing extension scripts if possible
        script_names = self.extension_manager.get_script_names()
        if hasattr(collection, "findScript") and hasattr(collection, "remove"):
            for name in script_names:
                try:
                    existing = collection.findScript(name)
                    if existing:
                        collection.remove(existing)
                except Exception:
                    pass

        for script in self.extension_manager.get_scripts():
            try:
                collection.insert(script)
            except Exception:
                pass

    def reload_extension_scripts(self):
        """Re-apply extension scripts to all profiles."""
        profiles = set()
        try:
            profiles.add(self.profile)
        except Exception:
            pass

        for i in range(self.tabs.count()):
            view = self.tabs.widget(i)
            if hasattr(view, "get_profile"):
                try:
                    profiles.add(view.get_profile())
                except Exception:
                    pass

        for profile in profiles:
            self._apply_extension_scripts_to_profile(profile)

    def _set_efficiency_mode(self, enabled: bool):
        """
        Set Windows Efficiency Mode (EcoQoS) for the current process.
        """
        try:
            # Constants
            ProcessPowerThrottling = 4
            PROCESS_POWER_THROTTLING_CURRENT_VERSION = 1
            PROCESS_POWER_THROTTLING_EXECUTION_SPEED = 1
            
            class PROCESS_POWER_THROTTLING_STATE(ctypes.Structure):
                _fields_ = [
                    ("Version", ctypes.c_ulong),
                    ("ControlMask", ctypes.c_ulong),
                    ("StateMask", ctypes.c_ulong),
                ]
            
            # Setup structure
            throttling = PROCESS_POWER_THROTTLING_STATE()
            throttling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION
            throttling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED
            
            if enabled:
                # Enable EcoQoS
                throttling.StateMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED
            else:
                # Disable EcoQoS (Normal mode)
                throttling.StateMask = 0
                
            # Define API types explicitly for 64-bit compatibility
            kernel32 = ctypes.windll.kernel32
            
            # SetProcessInformation
            # BOOL SetProcessInformation(HANDLE hProcess, PROCESS_INFORMATION_CLASS ProcessInformationClass, LPVOID ProcessInformation, DWORD ProcessInformationSize);
            kernel32.SetProcessInformation.argtypes = [
                ctypes.c_void_p,
                ctypes.c_int,
                ctypes.c_void_p,
                ctypes.c_uint32
            ]
            kernel32.SetProcessInformation.restype = ctypes.c_int
            
            # GetCurrentProcess
            kernel32.GetCurrentProcess.restype = ctypes.c_void_p
            
            handle = kernel32.GetCurrentProcess()
            
            ret = kernel32.SetProcessInformation(
                handle,
                ProcessPowerThrottling,
                ctypes.byref(throttling),
                ctypes.sizeof(throttling)
            )
            
            if ret == 0:
                err = ctypes.GetLastError()
                print(f"Failed to set Efficiency Mode: {err}")
                # Fallback: maybe we need a real handle? 
                # OpenProcess(PROCESS_SET_INFORMATION, FALSE, GetCurrentProcessId())
            else:
                status = "Enabled" if enabled else "Disabled"
                print(f"Efficiency Mode {status} successfully.")
                
        except Exception as e:
            print(f"Error setting efficiency mode: {e}")

    def _vertical_tabs_compact_mode(self):
        return (
            self.settings.get("vertical_tabs", False)
            and self.settings.get("vertical_tabs_collapsed", True)
            and not self.vertical_tabs_hover_expanded
        )

    def _vertical_tabs_width(self):
        if self.settings.get("vertical_tabs", False) and self.settings.get("vertical_tabs_collapsed", True):
            if self.vertical_tabs_hover_expanded:
                return self.vertical_tabs_expanded_width
            return self.vertical_tabs_collapsed_width
        return self.vertical_tabs_expanded_width

    def _apply_vertical_tabs_width(self):
        width = self._vertical_tabs_width()
        self.tabs.tab_bar.setFixedWidth(width)
        self.tabs.setMinimumWidth(0)
        self.tabs.setMaximumWidth(16777215)
        self.tabs.tab_bar.set_compact_mode(self._vertical_tabs_compact_mode())

    def _reset_vertical_tabs_width(self):
        self.tabs.tab_bar.setMinimumWidth(0)
        self.tabs.tab_bar.setMaximumWidth(16777215)
        self.tabs.setMinimumWidth(0)
        self.tabs.setMaximumWidth(16777215)
        self.tabs.tab_bar.set_compact_mode(False)

    def apply_vertical_tabs_setting(self):

        enabled = self.settings.get("vertical_tabs", False)
        if not hasattr(self, "tabs"):
            return

        try:
            tab_position_left = QTabWidget.TabPosition.West
            tab_position_top = QTabWidget.TabPosition.North
            tab_shape_west = QTabBar.Shape.RoundedWest
            tab_shape_north = QTabBar.Shape.RoundedNorth
        except AttributeError:
            tab_position_left = QTabWidget.Left
            tab_position_top = QTabWidget.Top
            tab_shape_west = QTabBar.RoundedWest
            tab_shape_north = QTabBar.RoundedNorth

        self.vertical_tabs_hover_expanded = False

        if enabled:
            self.tabs.setTabPosition(tab_position_left)
            self.tabs.tab_bar.setShape(tab_shape_west)
            self._apply_vertical_tabs_width()
        else:
            self.tabs.setTabPosition(tab_position_top)
            self.tabs.tab_bar.setShape(tab_shape_north)
            self._reset_vertical_tabs_width()

        if hasattr(self, "vertical_tabs_action"):
            self.vertical_tabs_action.setChecked(enabled)
            self.vertical_tabs_action.setText("Vertical Tabs")
        if hasattr(self, "vertical_tabs_collapse_action"):
            self.vertical_tabs_collapse_action.setChecked(self.settings.get("vertical_tabs_collapsed", True))
            self.vertical_tabs_collapse_action.setText("Collapse Vertical Tabs")
            self.vertical_tabs_collapse_action.setEnabled(enabled)

    def _on_vertical_tab_bar_hover_entered(self):
        if not self.settings.get("vertical_tabs", False):
            return
        if not self.settings.get("vertical_tabs_collapsed", True):
            return
        self.vertical_tabs_hover_expanded = True
        self._apply_vertical_tabs_width()

    def _on_vertical_tab_bar_hover_left(self):
        if not self.settings.get("vertical_tabs", False):
            return
        if not self.settings.get("vertical_tabs_collapsed", True):
            return
        self.vertical_tabs_hover_expanded = False
        self._apply_vertical_tabs_width()

    def _save_tab_settings(self):
        save_json_file(SETTINGS_FILE, self.settings)

    def toggle_vertical_tabs(self, enabled):
        self.settings["vertical_tabs"] = bool(enabled)
        self.apply_vertical_tabs_setting()
        self._save_tab_settings()

    def toggle_vertical_tabs_collapsed(self, collapsed):
        self.settings["vertical_tabs_collapsed"] = bool(collapsed)
        self.apply_vertical_tabs_setting()
        self._save_tab_settings()
    
    def show_menu(self):
        """Show main menu"""
        menu = QMenu(self)
        menu.addAction(self.new_tab_action)
        menu.addAction(self.new_window_action)
        menu.addAction(self.incognito_action)
        menu.addSeparator()
        menu.addAction(self.add_bookmark_action)
        menu.addAction(self.show_bookmarks_action)
        menu.addAction(self.show_history_action)
        menu.addAction(self.downloads_action)
        menu.addSeparator()
        menu.addAction(self.find_action)
        menu.addAction(self.vertical_tabs_action)
        menu.addAction(self.vertical_tabs_collapse_action)
        menu.addAction(self.dev_tools_action)
        menu.addAction(self.ai_sidebar_action)
        menu.addSeparator()
        menu.addAction(self.extensions_action)
        if hasattr(self, "permissions_action"):
            menu.addAction(self.permissions_action)
        menu.addAction(self.language_action)
        menu.addAction(self.settings_action)
        menu.addSeparator()
        menu.addAction(self.about_action)
        menu.addAction(self.save_page_action)
        menu.addSeparator()
        menu.addAction(self.exit_action)

        anchor = self.menu_btn if hasattr(self, "menu_btn") else self
        menu.exec(anchor.mapToGlobal(anchor.rect().bottomLeft()))
        
    def translate_page(self):
        """Translate the current page using Google Translate"""
        browser = self.current_browser()
        if not browser:
            return
            
        current_url = browser.url().toString()
        if "translate.google" in current_url or current_url.startswith("cloudar://") or current_url == "about:blank":
            return
            
        # target language could be configurable, default to auto->en (or system locale)
        target_lang = self.settings.get("target_language", "en")
        translate_url = f"https://translate.google.com/translate?sl=auto&tl={target_lang}&u={current_url}"
        
        browser.load(QUrl(translate_url))

    def toggle_find_bar(self):
        """Show/Hide find bar"""
        if self.find_bar.isVisible():
            self.find_bar.hide_bar()
        else:
            self.find_bar.show_bar()
            
    def zoom_page(self, step):
        """Zoom in/out"""
        browser = self.current_browser()
        if browser:
            current_zoom = browser.zoomFactor()
            browser.setZoomFactor(current_zoom + step)
            
    def reset_zoom(self):
        """Reset zoom to 100%"""
        browser = self.current_browser()
        if browser:
            browser.setZoomFactor(1.0)
            
    def save_page(self):
        """Save current page as HTML"""
        browser = self.current_browser()
        if browser:
            browser.page().toHtml(lambda html: self._save_html_callback(html))
            
    def _save_html_callback(self, html):
        import os
        from core.browser_qt import QFileDialog
        filename, _ = QFileDialog.getSaveFileName(self, get_text("save_page_dialog"), "", "HTML Files (*.html);;All Files (*)")
        if filename:
            with open(filename, 'w', encoding='utf-8') as f:
                f.write(html)
                
    def toggle_dev_tools(self):
        """Toggle DevTools as a docked panel (F12)"""
        browser = self.current_browser()
        if not browser:
            return

        # Enable developer extras on this page
        try:
            attr = getattr(QWebEngineSettings.WebAttribute, 'DeveloperExtrasEnabled', None)
            if attr:
                browser.page().settings().setAttribute(attr, True)
        except Exception:
            pass

        # Create the dock once
        if self._devtools_dock is None:
            self._devtools_view = QWebEngineView()
            self._devtools_dock = QDockWidget(get_text("devtools_title"), self)
            self._devtools_dock.setWidget(self._devtools_view)
            self._devtools_dock.setMinimumHeight(250)
            self._devtools_dock.setFeatures(
                QDockWidget.DockWidgetFeature.DockWidgetClosable |
                QDockWidget.DockWidgetFeature.DockWidgetMovable |
                QDockWidget.DockWidgetFeature.DockWidgetFloatable
            )
            self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea,
                               self._devtools_dock)

        if self._devtools_dock.isVisible():
            self._devtools_dock.hide()
        else:
            self._attach_devtools_to(browser)
            self._devtools_dock.show()

    def _attach_devtools_to(self, browser):
        """Point the shared DevTools panel at the given browser tab."""
        if self._devtools_view:
            try:
                browser.page().setDevToolsPage(self._devtools_view.page())
            except Exception:
                pass

    def toggle_media_control(self):
        """Show media control popup"""
        browser = self.current_browser()
        if not browser:
            return

        # Initialize popup if not exists
        if not hasattr(self, 'media_popup'):
            self.media_popup = MediaControlPopup(self)
            self.media_popup.play_pause_clicked.connect(self._on_media_play_pause)
            self.media_popup.prev_clicked.connect(self._on_media_prev)
            self.media_popup.next_clicked.connect(self._on_media_next)
            
        # Position popup below the button
        # (Simple positioning, can be improved)
        # Position popup below the button
        # Use mapToGlobal on the button itself
        global_pos = self.media_btn.mapToGlobal(self.media_btn.rect().bottomLeft())
        # Offset slightly to align
        global_pos.setY(global_pos.y() + 5)
        # Shift left to right-align if it goes off screen (optional, but good for right-side buttons)
        global_pos.setX(global_pos.x() - self.media_popup.width() + self.media_btn.width())
        
        self.media_popup.move(global_pos)
        
        if self.media_popup.isVisible():
            self.media_popup.hide()
        else:
            self.update_media_metadata(browser)
            self.media_popup.show()
            self.media_popup.raise_()

    def _on_media_play_pause(self):
        browser = self.current_browser()
        if browser:
            browser.triggerPageAction(QWebEnginePage.WebAction.ToggleMediaPlayPause)
            # Re-check metadata to update icon state
            QTimer.singleShot(500, lambda: self.update_media_metadata(browser))

    def _on_media_prev(self):
        # WebEngine doesn't have a direct "Prev Track" action exposed easily via WebAction
        # We try to inject JS or use standard media key simulation if possible.
        # For now, minimal implementation:
        pass

    def _on_media_next(self):
        pass

    def update_media_metadata(self, browser):
        """Inject JS to get current media info"""
        js_code = """
        (function() {
            var meta = navigator.mediaSession.metadata;
            var title = meta ? meta.title : document.title;
            var artist = meta ? meta.artist : '';
            var paused = true;
            
            var media = document.querySelector('video, audio');
            if (media) {
                paused = media.paused;
            }
            
            return {
                title: title,
                artist: artist,
                paused: paused
            };
        })();
        """
        if browser:
            browser.page().runJavaScript(js_code, self._handle_metadata_result)
            
    def _handle_metadata_result(self, result):
        if hasattr(self, 'media_popup') and isinstance(result, dict):
            self.media_popup.update_metadata(result)

    def _update_media_button_visibility(self):
        """Check if any tab is audible"""
        is_audible = False
        for i in range(self.tabs.count()):
            page = self.tabs.widget(i).page()
            if page.recentlyAudible():
                is_audible = True
                break
        if hasattr(self, 'media_btn'):
            self.media_btn.setVisible(is_audible)

    def _on_adblock_count_changed(self, count: int):
        """Update status bar AdBlock counter."""
        if hasattr(self, '_adblock_lbl'):
            self._adblock_lbl.setText(f"AdBlock: {count}")

    def toggle_split_screen(self):
        """Open a Split Screen tab with the current page on the left."""
        from core.split_view import SplitBrowserView
        browser = self.current_browser()
        current_url = browser.url() if browser else QUrl("cloudar://newtab")

        split = SplitBrowserView(
            profile=None,          # isolated profiles per pane
            left_url=current_url,
            right_url=QUrl("cloudar://newtab"),
        )

        # Wrap SplitBrowserView as a tab
        i = self.tabs.addTab(split, f"\u29c9 {get_text('split_tab_title')}")
        self.tabs.setCurrentIndex(i)

    def save_current_session(self):
        """Save currently open tabs"""
        if self.is_incognito:
            return
        tabs_data = []
        for i in range(self.tabs.count()):
            browser = self.tabs.widget(i)
            tabs_data.append(browser.url().toString())
            
        # Add to settings or separate session file
        self.settings["last_session"] = tabs_data
        save_json_file(SETTINGS_FILE, self.settings)
        
    def _is_valid_startup_url(self, url):
        if not url or url == "about:blank":
            return True
        qurl = QUrl(url)
        if qurl.scheme() == "cloudar":
            return qurl.host() in {
                "about", "bookmarks", "downloads", "extensions",
                "history", "newtab", "permissions", "settings"
            }
        return True

    def restore_last_session(self):
        """Restore tabs from last session"""
        if self.is_incognito:
            return
        last_session = self.settings.get("last_session", [])
        if last_session:
            for url in last_session:
                if self._is_valid_startup_url(url):
                    self.add_new_tab(QUrl(url))
                else:
                    self.add_new_tab(QUrl(self._get_homepage_url()))

    def apply_startup_behavior(self):
        """Apply startup behavior: continue, homepage, or new tab."""
        startup = self.settings.get("startup_behavior", "Open New Tab")

        if startup == "Continue where you left off" and not self.is_incognito:
            self.restore_last_session()
            if self.tabs.count() > 0:
                return

        if startup == "Open Home Page":
            self.add_new_tab(QUrl(self._get_homepage_url()))
        else:
            # "Open New Tab" ” use the configured homepage instead of hardcoding cloudar://newtab
            self.add_new_tab(QUrl(self._get_homepage_url()))
    
    def toggle_privacy_quick_action(self):
        """Toggle privacy quick action mode.
        
        When activated, forces extreme privacy settings on all tabs:
        - Enables fingerprinting protection
        - Enables tracking script blocking  
        - Enables ad blocking
        - Clears all cookies for the current session
        - Enforces HTTPS
        - Blocks third-party cookies
        """
        self._privacy_quick_action_active = not self._privacy_quick_action_active
        if hasattr(self, 'privacy_btn'):
            self.privacy_btn.setChecked(self._privacy_quick_action_active)
            if self._privacy_quick_action_active:
                self.privacy_btn.setToolTip("Anonymity Mode ACTIVE - Click to disable")
            else:
                self.privacy_btn.setToolTip("Anonymity Mode - Click to enable")
        
        if self._privacy_quick_action_active:
            # Apply extreme privacy settings
            self.settings["block_fingerprinting"] = True
            self.settings["block_tracking_scripts"] = True
            self.settings["block_third_party_ads"] = True
            self.settings["third_party_cookies"] = "block"
            self.settings["absolute_security"] = True
            
            # Apply to all tabs
            for i in range(self.tabs.count()):
                browser = self.tabs.widget(i)
                if hasattr(browser, 'apply_security_settings'):
                    browser.apply_security_settings(self.settings)
                if hasattr(browser, '_apply_anonymity_settings'):
                    browser._apply_anonymity_settings(self.settings)
            
            self.apply_privacy_settings()
            self.status.showMessage("🔒 Anonymity Mode ACTIVE - Tracking & fingerprinting blocked", 3000)
        else:
            # Restore normal settings
            self.settings["absolute_security"] = False
            
            # Apply restored settings
            for i in range(self.tabs.count()):
                browser = self.tabs.widget(i)
                if hasattr(browser, 'apply_security_settings'):
                    browser.apply_security_settings(self.settings)
                if hasattr(browser, '_apply_anonymity_settings'):
                    browser._apply_anonymity_settings(self.settings)
            
            self.apply_privacy_settings()
            self.status.showMessage("Anonymity Mode disabled", 2000)

    def apply_stylesheet(self):
        """Apply QSS stylesheet"""
        theme = "incognito" if self.is_incognito else self.settings.get("theme", "Dark").lower()
        self.setStyleSheet(get_stylesheet(theme))



