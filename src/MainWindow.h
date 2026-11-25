#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QToolBar>
#include <QMenuBar>
#include <QProgressBar>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebChannel>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QPoint>
#include <QRect>

#include "BrowserTabWidget.h"
#include "BrowserSidebar.h"
#include "DownloadManager.h"
#include "HistoryManager.h"
#include "SessionManager.h"
#include "BookmarkManager.h"
#include "BookmarkBar.h"
#include "ExtensionManager.h"
#include "WebExtensionManager.h"
#include "NativeBridge.h"
#include "InternalBridge.h"
#include "NewTabBridge.h"
#include "SettingsBridge.h"
#include "SpinnerWidget.h"
#include "TitleBar.h"
#include "AdBlock.h"
#include "AISidebar.h"
#include "SplitView.h"
#include "YouTubeDownloader.h"
#include "TorrentDownloader.h"
#include "IdleEasterEgg.h"
#include "MemorySaver.h"
#include "PerformanceProfile.h"
#include "ClassicTheme.h"

class QSplitter;
class QMenu;
// Overridden events: declared rather than included, so MainWindow.h keeps its
// include list small. QWidget.h does not pull in every event type.
class QCloseEvent;
class QResizeEvent;
class LocalFileService;
class NotificationPresenter;
class WebShortcutStore;
class QToolBar;
class QToolButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    // `mirrorWindow` marks a window that was opened for one torn-off tab (see
    // tearOffTab). Such a window neither restores the saved session — it is not
    // a fresh start — nor writes it back, because the session is one list of
    // URLs and two windows writing it would take turns clobbering each other.
    explicit MainWindow(bool isIncognito = false, QWidget *parent = nullptr,
                        bool mirrorWindow = false);

    // A "web shortcut" window (roadmap item 7): one site, no tabs, no omnibox,
    // no menus beyond a reload/back row, and a title bar that shows the
    // shortcut's name. See src/WebShortcutStore.h for what this is not.
    //
    // Built by createAppWindow(), which constructs a normal window and then
    // replaces its tab surface: the profile, scheme handlers, ad-block
    // interceptor and QWebChannel are exactly the ones a normal window has, and
    // duplicating that setup for a second window model is how the two drift
    // apart (see docs/KNOWN_ISSUES.md §11 for the same trade on tear-off).
    static MainWindow *createAppWindow(const QString &name, const QUrl &url,
                                       const QString &shortcutId,
                                       QWidget *parent = nullptr);
    bool isAppWindow() const { return m_isAppWindow; }
    QString shortcutId() const { return m_shortcutId; }

    void openUrl(const QUrl &url);
    void setSidebarVisible(bool visible);
    MemorySaver *memorySaver() const { return m_memorySaver; }
    bool isIncognito() const { return m_isIncognito; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    // Notification toasts are positioned in window coordinates, so they have to
    // be re-stacked when the window changes size.
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

public slots:
    // Opens (or focuses) the DevTools window for the current tab. Public because
    // both the F12 shortcut and the context menu reach it.
    void openDevToolsForCurrentTab();
    // Where the sidebar's tear-off gesture lands: the tab is removed from this
    // window and rebuilt in a new one. MainWindow owns the window-creation
    // policy, which is why the sidebar only reports the gesture.
    void tearOffTab(int index);

private slots:
    void navigateToAddressBarInput();
    void updateAddressBar(const QUrl &url);
    void updateWindowTitle(const QString &title);
    // Same title text without the history side effect (see the definition).
    void displayWindowTitle(const QString &title);
    void updateProgress(int percent);
    void showHistoryDialog();
    void openSettingsPage(); // opens/focuses a tab on cloudar://settings/
    // Local files (roadmap item 5). "Open File..." loads the picker's file://
    // URL in a new tab; "Open Folder..." opens the folder's cloudar://files
    // listing, which is also what makes that folder listable at all — see
    // LocalFileService's confinement rule.
    void openLocalFile();
    void openLocalFolder();
    // A notification was activated: raise the tab whose page sent it.
    void focusPageForNotification(const QUrl &origin);
    // "Create web shortcut..." for the current page, and the menu that launches
    // the ones that already exist.
    void createShortcutForCurrentPage();
    void openShortcutById(const QString &id);
    void showShortcutMenu();
    // cloudar://files asked for the folder picker (see requestFolderPicker):
    // the chosen folder replaces the current listing instead of opening a
    // second tab.
    void pickFolderForFilesPage();
    // Page context menu helpers (see PageContextMenu).
    void openPageSource(const QUrl &url);
    void openInNewWindow(const QUrl &url);
    void showExtensionsDialog();
    void toggleBookmarkForCurrentPage();
    void updateBookmarkStar(const QUrl &url);
    void updateNavigationState();
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    void requestPermission(const QWebEnginePermission &permission);
#else
    void handleFeaturePermission(const QUrl &origin, QWebEnginePage::Feature feature, QWebEnginePage *page);
#endif
    void saveSession();

private:
    QWebEngineProfile *m_profile = nullptr;
    BrowserTabWidget *m_tabs = nullptr;
    BrowserSidebar *m_sidebar = nullptr;
    QSplitter *m_browserSplitter = nullptr;
    QLineEdit *m_addressBar = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPropertyAnimation *m_progressAnim = nullptr;
    QGraphicsDropShadowEffect *m_addressGlow = nullptr;
    QPropertyAnimation *m_addressGlowAnim = nullptr;
    SpinnerWidget *m_spinner = nullptr;
    DownloadManager *m_downloadManager = nullptr;
    HistoryManager *m_historyManager = nullptr;
    SessionManager *m_sessionManager = nullptr;
    BookmarkManager *m_bookmarkManager = nullptr;
    BookmarkBar *m_bookmarkBar = nullptr;
    QAction *m_bookmarkStarAction = nullptr;
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_reloadStopAction = nullptr;
    // Every raw pointer above and below starts as nullptr. This is not tidiness:
    // the classic chrome never creates the drop-shadow effect or the toolbar
    // progress bar, so without the initialiser applyTheme() dereferenced
    // whatever happened to be in that member and the browser segfaulted four
    // seconds into every classic-chrome start.
    QAction *m_sidebarAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomResetAction = nullptr;
    ExtensionManager *m_extensionManager = nullptr;
    WebExtensionManager *m_webExtensionManager = nullptr; // real, manifest.json-based Chrome-style extensions
    QWebChannel *m_webChannel = nullptr;
    NativeBridge *m_nativeBridge = nullptr;
    // Backs cloudar://bookmarks, /history, /downloads, /extensions and
    // /permissions. Registered next to `native` on the same channel, which is
    // only ever attached to internal pages.
    InternalBridge *m_internalBridge = nullptr;
    // Real favicons for internal pages, fed by BrowserTabWidget::wireView() and
    // read by NewTabBridge / LightSchemeHandler.
    FaviconStore *m_faviconStore = nullptr;
    NewTabBridge *m_newTabBridge = nullptr;
    // Survives from the address bar to the page: the service is what remembers
    // which folders the user opened, so cloudar://files keeps working after the
    // tab that opened the folder is gone.
    LocalFileService *m_localFiles = nullptr;
    // Web notifications (roadmap item 6). Owns the toasts and the profile's
    // notification presenter; see NotificationPresenter.h for what this build
    // does *not* do with them.
    NotificationPresenter *m_notifications = nullptr;
    // Web shortcuts (roadmap item 7). QSettings-backed, see WebShortcutStore.h.
    WebShortcutStore *m_shortcuts = nullptr;
    SettingsBridge *m_settingsBridge = nullptr;
    // Both are null on the classic chrome, which uses the OS window frame and
    // QMainWindow's own menu bar; everything that touches them checks first.
    TitleBar *m_titleBar = nullptr;
    // The modern chrome's whole menu lives in this one QMenu, attached to the
    // title bar's three-dot button. Kept so it can be destroyed and rebuilt when
    // the interface language changes.
    QMenu *m_modernMenu = nullptr;
    bool m_menuBuilt = false;
    QMenuBar *m_menuBar = nullptr; // owned manually (not via menuBar()) so it can be nested below m_titleBar
    Qt::Edges m_resizeEdges;
    QPoint m_resizeStartGlobal;
    QRect m_resizeStartGeometry;
    bool m_resizing = false;
    bool m_isIncognito = false;
    // See the constructor: a window that exists to hold one torn-off tab.
    bool m_mirrorWindow = false;
    // Set only for a web-shortcut window: the id of the shortcut it shows, so
    // "Open at login"-style re-launches and the shortcut menu can tell which
    // one this is. Empty in a normal window.
    QString m_shortcutId;
    bool m_isAppWindow = false;
    // Where a shortcut window's size is remembered (empty in a normal window).
    QString m_appGeometryKey;
    bool m_firstShowDone = false;
    QVariantAnimation *m_sidebarAnimation = nullptr;

    // Cloudar features
    AdBlockInterceptor *m_adBlockInterceptor = nullptr;
    AISidebar *m_aiSidebar = nullptr;
    QAction *m_aiSidebarAction = nullptr;
    SplitView *m_splitView = nullptr;
    MemorySaver *m_memorySaver = nullptr;
    QToolButton *m_classicPageIcon = nullptr;   // blue page glyph inside the omnibox
    QToolButton *m_classicGoButton = nullptr;   // the 2009 "go" arrow
    QAction *m_homeAction = nullptr;
    QAction *m_themeClassicAction = nullptr;
    QAction *m_themeModernAction = nullptr;
    QAction *m_splitViewAction = nullptr;     // toolbar button (modern chrome only)
    QAction *m_splitViewMenuAction = nullptr; // View > Split view, both chromes
    YouTubeDownloaderBridge *m_youtubeBridge = nullptr;
    TorrentDownloaderBridge *m_torrentBridge = nullptr;
    IdleEasterEgg *m_idleEasterEgg = nullptr;

    void buildTitleBar();  // frameless window chrome: title bar row + real menu bar, stacked as one unit
    void applyChromeStyle();      // swaps frameless/modern for the classic windowed chrome
    void buildClassicOmniboxDecor(); // page icon + go button inside the address field
    QToolBar *m_navToolbar = nullptr;
    QToolBar *m_bookmarkToolbar = nullptr;
    void toggleSplitView(bool checked);
    // The QWebEngineView navigation should act on: the split view's focused
    // pane while it is up, otherwise the tab widget's current tab. Returns null
    // when there is nothing to act on, so every caller can bail out the same way.
    QWebEngineView *activeView() const;

    // Hands the light/dark decision to Chromium (page background, form controls
    // and scrollbars), which QSS cannot reach. Called at startup and when the
    // user changes the colour mode.
    void applyPageColorScheme();
    // One message for both menu builders: they must not disagree about where
    // the restart entry lives, which is exactly how the modern chrome ended up
    // pointing at a Tools menu it does not have.
    void announceStyleChange(Theme::ThemeStyle style);

    // Language. QTranslator swaps the strings; these three make the *already
    // built* widgets pick them up, which is the part Qt does not do for you.
    void retranslateUi();          // menus, toolbar text, status bar, tab strip
    void buildMenuOnce();          // the menus are built exactly once per window
    void clearMenuUi();            // destroy the menus before rebuilding them
    void applyMenuTranslations();  // the handful of strings outside the menus
    void refreshStatusBar();
    void buildModernMenu();       // menu contents for the modern chrome
    void buildClassicMenu();      // Chrome 2009-style Page/Tools menus
    void buildToolbar();
    void buildSidebar();
    // App-window mode: one page, a back/forward/reload row, and the shortcut's
    // name as the title. See WebShortcutStore.h for what an app window is not.
    void buildAppWindow(const QString &name, const QUrl &url);
    void buildBookmarksBar(); // second toolbar row, directly below the nav toolbar
    void buildMenu();
    void applySettings(); // pull home page / search engine / cookie policy from QSettings
    void applyTheme();    // re-apply the accent-color stylesheet app-wide
    void showFindBar();
    void setZoom(qreal zoom);
    void setSidebarExpanded(bool expanded, bool animated = true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QString permissionName(QWebEnginePermission::PermissionType type) const;
#else
    QString featureName(QWebEnginePage::Feature feature) const;
#endif
    Qt::Edges resizeEdgesAt(const QPoint &globalPos) const;
    void updateResizeCursor(const QPoint &globalPos);
    void resizeFromGlobalPos(const QPoint &globalPos);
};
