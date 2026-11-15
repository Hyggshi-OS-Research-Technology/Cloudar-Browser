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
#include "SettingsBridge.h"
#include "SpinnerWidget.h"
#include "TitleBar.h"
#include "AdBlock.h"
#include "AISidebar.h"
#include "SplitView.h"
#include "YouTubeDownloader.h"
#include "TorrentDownloader.h"
#include "IdleEasterEgg.h"

class QSplitter;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(bool isIncognito = false, QWidget *parent = nullptr);

    void openUrl(const QUrl &url);
    void setSidebarVisible(bool visible);
    bool isIncognito() const { return m_isIncognito; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void navigateToAddressBarInput();
    void updateAddressBar(const QUrl &url);
    void updateWindowTitle(const QString &title);
    void updateProgress(int percent);
    void showHistoryDialog();
    void openSettingsPage(); // opens/focuses a tab on cloudar://settings/
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
    QWebEngineProfile *m_profile;
    BrowserTabWidget *m_tabs;
    BrowserSidebar *m_sidebar;
    QSplitter *m_browserSplitter;
    QLineEdit *m_addressBar;
    QProgressBar *m_progressBar;
    QPropertyAnimation *m_progressAnim;
    QGraphicsDropShadowEffect *m_addressGlow;
    QPropertyAnimation *m_addressGlowAnim;
    SpinnerWidget *m_spinner;
    DownloadManager *m_downloadManager;
    HistoryManager *m_historyManager;
    SessionManager *m_sessionManager;
    BookmarkManager *m_bookmarkManager;
    BookmarkBar *m_bookmarkBar;
    QAction *m_bookmarkStarAction;
    QAction *m_backAction;
    QAction *m_forwardAction;
    QAction *m_reloadStopAction;
    // Created later in buildMenu(), while the sidebar preference is applied
    // during buildSidebar(). Initialise explicitly so startup cannot inspect
    // an indeterminate QAction pointer.
    QAction *m_sidebarAction = nullptr;
    QAction *m_findAction;
    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_zoomResetAction;
    ExtensionManager *m_extensionManager;
    WebExtensionManager *m_webExtensionManager; // real, manifest.json-based Chrome-style extensions
    QWebChannel *m_webChannel;
    NativeBridge *m_nativeBridge;
    SettingsBridge *m_settingsBridge;
    TitleBar *m_titleBar;
    QMenuBar *m_menuBar; // owned manually (not via menuBar()) so it can be nested below m_titleBar
    Qt::Edges m_resizeEdges;
    QPoint m_resizeStartGlobal;
    QRect m_resizeStartGeometry;
    bool m_resizing = false;
    bool m_isIncognito = false;
    bool m_firstShowDone = false;
    QVariantAnimation *m_sidebarAnimation = nullptr;

    // Cloudar features
    AdBlockInterceptor *m_adBlockInterceptor = nullptr;
    AISidebar *m_aiSidebar = nullptr;
    QAction *m_aiSidebarAction = nullptr;
    SplitView *m_splitView = nullptr;
    QAction *m_splitViewAction = nullptr;
    YouTubeDownloaderBridge *m_youtubeBridge = nullptr;
    TorrentDownloaderBridge *m_torrentBridge = nullptr;
    IdleEasterEgg *m_idleEasterEgg = nullptr;

    void buildTitleBar();  // frameless window chrome: title bar row + real menu bar, stacked as one unit
    void buildToolbar();
    void buildSidebar();
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
