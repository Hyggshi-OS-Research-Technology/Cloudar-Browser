#include "MainWindow.h"
#include "SettingsDialog.h"
#include "ExtensionsDialog.h"
#include "LightSchemeHandler.h"
#include "ThemeManager.h"
#include "ReaderMode.h"
#include "AdBlock.h"
#include "AISidebar.h"
#include "SplitView.h"
#include "YouTubeDownloader.h"
#include "TorrentDownloader.h"
#include "IdleEasterEgg.h"
#include "InternalBridge.h"
#include "PermissionStore.h"

#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QSizeGrip>
#include <QAction>
#include <QActionGroup>
#include <QStandardPaths>
#include <QDir>
#include <QListWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QInputDialog>
#include <QMessageBox>
#include <QWebEngineSettings>
#include <QWebEngineHistory>
#include <QSettings>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTimer>
#include <QShowEvent>
#include <QToolButton>
#include <QTabBar>
#include <QSignalBlocker>
#include <QSysInfo>

MainWindow::MainWindow(bool isIncognito, QWidget *parent)
    : QMainWindow(parent), m_isIncognito(isIncognito)
{
    setWindowIcon(QIcon(":/icon.png"));

    // Two chromes, two window models:
    //
    //   Modern  - frameless, so the title bar can be recolored with the accent
    //             colour (window managers own the native decoration and give
    //             Qt no portable way to tint it).
    //   Classic - a normal decorated window, because Chrome 0.2/1.0 on 2009
    //             Windows looked exactly like every other Win32 app: Aero/
    //             Classic title bar, menu row, tab strip, toolbar, status bar.
    //             Faking that inside a frameless window would only fight the
    //             platform's own window manager.
    const Theme::Metrics metrics = Theme::metrics();
    setWindowFlag(Qt::FramelessWindowHint, metrics.frameless);
    if (m_isIncognito)
        setAttribute(Qt::WA_DeleteOnClose, true);
    if (metrics.frameless) {
        // A frameless window has no window-manager resize border. Listening at
        // the application level keeps the edges usable over web content and
        // toolbars as well as the otherwise empty window frame.
        qApp->installEventFilter(this);
    }
    // Both chromes are painted by the application stylesheet
    // (Theme::appStyleSheet); no per-window override is applied, so dialogs and
    // menus keep whatever the platform style provides on top of it.

    if (m_isIncognito) {
        // Pure in-memory off-the-record profile: no history, no persistent disk cookies
        m_profile = new QWebEngineProfile(this);
        m_profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        m_profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    } else {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/webengine";
        QDir().mkpath(dataDir);
        m_profile = new QWebEngineProfile("LightBrowserProfile", this);
        m_profile->setPersistentStoragePath(dataDir);
        m_profile->setCachePath(dataDir + "/cache");
        m_profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
        // Cap the disk cache so the browser stays "light" on storage too.
        m_profile->setHttpCacheMaximumSize(100 * 1024 * 1024); // 100 MB
    }

    // Media & video playback settings
    m_profile->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    m_profile->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    m_profile->settings()->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    m_profile->settings()->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);
    // Internal pages (cloudar://) are served through LightSchemeHandler and are
    // treated by Chromium as local content. The New Tab page lets the user point
    // a wallpaper at a remote image URL, so local content must be allowed to
    // reach the network — that part is by design.
    m_profile->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    // The opposite direction is not needed by anything in the browser and is a
    // real risk: without this, a deliberately-crafted `cloudar://` page or a
    // local file opened by the user cannot read arbitrary files from disk.
    m_profile->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    // Mixed content stays blocked. Allowing HTTP subresources inside an HTTPS
    // page silently downgrades transport security, and nothing in the browser
    // depends on it.
    m_profile->settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, false);

    m_profile->installUrlSchemeHandler("light", new LightSchemeHandler(this));
    m_profile->installUrlSchemeHandler("browser", new LightSchemeHandler(this));
    m_profile->installUrlSchemeHandler("cloudar", new LightSchemeHandler(this));

    // AdBlock: install request interceptor on profile
    m_adBlockInterceptor = new AdBlockInterceptor(this);
    m_profile->setUrlRequestInterceptor(m_adBlockInterceptor);

    m_downloadManager = new DownloadManager(this, this);
    m_historyManager = new HistoryManager(this);
    m_sessionManager = new SessionManager(this);
    m_bookmarkManager = new BookmarkManager(this);
    m_extensionManager = new ExtensionManager(m_profile, this);
    m_webExtensionManager = new WebExtensionManager(m_profile, this);

    connect(m_profile, &QWebEngineProfile::downloadRequested, m_downloadManager,
            &DownloadManager::handleDownloadRequested);

    // Native bridge exposed only to cloudar:// internal pages (see
    // BrowserTabWidget::wireView) so the New Tab page's tiles can open
    // native dialogs instead of navigating.
    m_webChannel = new QWebChannel(this);
    m_nativeBridge = new NativeBridge(this);
    m_webChannel->registerObject("native", m_nativeBridge);
    // The five "manager" internal pages (bookmarks, history, downloads,
    // extensions, permissions) all call into a QWebChannel object named
    // `internalBridge`; it existed in the pages but not in the browser, so each
    // of them rendered its empty state forever. Same channel as `native`, and
    // for the same reason: BrowserTabWidget only attaches it to cloudar:// pages.
    m_internalBridge = new InternalBridge(m_bookmarkManager, m_historyManager,
                                          m_downloadManager, m_webExtensionManager, this);
    m_webChannel->registerObject("internalBridge", m_internalBridge);
    connect(m_nativeBridge, &NativeBridge::historyRequested, this, &MainWindow::showHistoryDialog);
    connect(m_nativeBridge, &NativeBridge::downloadsRequested, this,
            [this]() { m_downloadManager->showDialog(); });
    connect(m_nativeBridge, &NativeBridge::settingsRequested, this, &MainWindow::openSettingsPage);
    connect(m_nativeBridge, &NativeBridge::extensionsRequested, this, &MainWindow::showExtensionsDialog);

    // Backs the cloudar://settings page (see resources/settings.html) — a
    // second QWebChannel object alongside "native" so the settings UI can
    // read/write real persisted settings and trigger native pickers,
    // without exposing any of that to ordinary websites (BrowserTabWidget
    // only attaches this channel to cloudar:// pages either way).
    m_settingsBridge = new SettingsBridge(m_profile, m_historyManager, this, this);
    m_webChannel->registerObject("settingsBridge", m_settingsBridge);

    // Cloudar feature bridges registered on the same channel
    m_youtubeBridge = new YouTubeDownloaderBridge(this, this);
    m_torrentBridge = new TorrentDownloaderBridge(this, this);
    m_webChannel->registerObject("youtube", m_youtubeBridge);
    m_webChannel->registerObject("torrent", m_torrentBridge);

    m_tabs = new BrowserTabWidget(m_profile, m_webChannel, this);
    m_tabs->setWebExtensionManager(m_webExtensionManager);

    // chrome.tabs.query() backing data — a plain JSON snapshot of open tabs,
    // rebuilt on demand (not cached) since it's only read when an
    // extension actually calls the API.
    m_webExtensionManager->setTabsProvider([this]() {
        QJsonArray arr;
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *view = qobject_cast<QWebEngineView *>(m_tabs->widget(i));
            if (!view)
                continue;
            QJsonObject tab;
            tab["id"] = i;
            tab["url"] = view->url().toString();
            tab["title"] = view->title();
            tab["active"] = (i == m_tabs->currentIndex());
            arr.append(tab);
        }
        return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    });

    // chrome.tabs.create/update/remove and chrome.scripting.executeScript —
    // both gated behind manifest permissions inside ChromeExtensionAPI
    // itself, this is just the "how" once something is allowed.
    m_webExtensionManager->setTabController(
        [this](const QString &url, bool active) {
            const QUrl target = url.isEmpty() ? QUrl("cloudar://newtab") : QUrl(url);
            return m_tabs->openTabForExtension(target, active);
        },
        [this](int tabId, const QString &url) {
            return m_tabs->updateTabForExtension(tabId, QUrl(url));
        },
        [this](int tabId) {
            return m_tabs->closeTabForExtension(tabId);
        });
    m_webExtensionManager->setTabUrlLookup([this](int tabId) {
        return m_tabs->urlForTabIndex(tabId);
    });
    m_webExtensionManager->setScriptRunner(
        [this](int tabId, const QString &code, std::function<void(const QString &)> callback) {
            m_tabs->runJavaScriptInTab(tabId, code, std::move(callback));
            return true;
        });

    // Memory Saver: the real implementation behind the settings page's
    // Light/Medium/High/Ultra levels (see MemorySaver.h). It reacts to tab
    // switches and periodically discards renderers that have been idle for
    // longer than the configured threshold.
    m_memorySaver = new MemorySaver(m_tabs, this);
    connect(m_tabs, &BrowserTabWidget::currentTabChanged, m_memorySaver,
            &MemorySaver::noteTabActivated);
    connect(m_settingsBridge, &SettingsBridge::settingsChanged, m_memorySaver,
            &MemorySaver::reloadSettings);

    connect(m_tabs, &BrowserTabWidget::urlChangedForCurrentTab, this, &MainWindow::updateAddressBar);
    connect(m_tabs, &BrowserTabWidget::urlChangedForCurrentTab, this, &MainWindow::updateBookmarkStar);
    connect(m_tabs, &BrowserTabWidget::titleChangedForCurrentTab, this, &MainWindow::updateWindowTitle);
    connect(m_tabs, &BrowserTabWidget::loadProgressForCurrentTab, this, &MainWindow::updateProgress);
    connect(m_tabs, &BrowserTabWidget::loadStartedForCurrentTab, this, [this]() {
        if (m_spinner)
            m_spinner->start();
        if (m_addressGlow && m_addressGlowAnim) {
            m_addressGlow->setEnabled(true);
            m_addressGlowAnim->start();
        }
        if (statusBar() && Theme::current() == Theme::ThemeStyle::Classic)
            statusBar()->showMessage(tr("Loading..."));
        updateNavigationState();
    });
    connect(m_tabs, &BrowserTabWidget::loadFinishedForCurrentTab, this, [this](bool ok) {
        if (m_spinner)
            m_spinner->stop();
        if (m_addressGlow && m_addressGlowAnim) {
            m_addressGlowAnim->stop();
            m_addressGlow->setBlurRadius(0);
            m_addressGlow->setEnabled(false);
        }
        if (statusBar() && Theme::current() == Theme::ThemeStyle::Classic)
            statusBar()->showMessage(ok ? tr("Done") : tr("Failed to open page"), 3000);
        updateProgress(100);
        updateNavigationState();
    });
    connect(m_tabs, &BrowserTabWidget::currentTabChanged, this, &MainWindow::updateNavigationState);
    connect(m_tabs, &BrowserTabWidget::tabsChanged, this, &MainWindow::saveSession);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(m_tabs, &BrowserTabWidget::permissionRequested, this,
            &MainWindow::requestPermission);
#else
    connect(m_tabs, &BrowserTabWidget::featurePermissionRequested, this,
            &MainWindow::handleFeaturePermission);
#endif

    buildTitleBar();
    buildToolbar();
    buildSidebar();
    buildBookmarksBar();
    buildMenu();
    applySettings();
    applyChromeStyle();
    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::accentChanged, this,
            [this](const QColor &) { applyTheme(); });

    // Size grip in a slim status bar — the only way left to resize once the
    // window is frameless and the OS border is gone. A decorated (classic)
    // window keeps the platform's own resize border, so a grip there would be
    // redundant chrome that Chrome 2009 never had.
    const Theme::Metrics chromeMetrics = Theme::metrics();
    if (chromeMetrics.frameless) {
        auto *grip = new QSizeGrip(this);
        statusBar()->addPermanentWidget(grip);
        statusBar()->setFixedHeight(16);
        statusBar()->setSizeGripEnabled(false);
    } else {
        // Decorated window: the platform border handles resizing, so the grip
        // would be redundant chrome Chrome 2009 never had.
        statusBar()->setSizeGripEnabled(true);
        statusBar()->setFixedHeight(20);
        statusBar()->showMessage(tr("Ready"));
    }

    // AISidebar dock widget
    m_aiSidebar = new AISidebar(this, this);
    m_aiSidebar->hide();
    addDockWidget(Qt::RightDockWidgetArea, m_aiSidebar);

    // Memory Saver feedback lives in the status bar on the classic chrome,
    // which is the only persistent strip of text it has.
    connect(m_memorySaver, &MemorySaver::statisticsChanged, this,
            [this](int sleptTabs, qint64 freedBytes) {
                const int mb = static_cast<int>(freedBytes / (1024 * 1024));
                const QString msg = sleptTabs > 0
                    ? tr("Memory Saver: %1 tab(s) sleeping, ~%2 MB released").arg(sleptTabs).arg(mb)
                    : tr("Memory Saver: no tabs are sleeping");
                statusBar()->showMessage(msg, 4000);
            });

    // AdBlock: show blocked count in status bar
    connect(m_adBlockInterceptor, &AdBlockInterceptor::blockedCountChanged, this, [this](int count) {
        statusBar()->showMessage(QString("🛡 %1 ads blocked").arg(count), 2000);
    });

    // IdleEasterEgg
    m_idleEasterEgg = new IdleEasterEgg(this, this);

    if (!m_isIncognito && m_sessionManager->restoreEnabled() && !m_sessionManager->urls().isEmpty())
        m_tabs->restoreTabs(m_sessionManager->urls(), m_sessionManager->currentIndex());
    else
        m_tabs->addNewTab(QUrl(SettingsDialog(this).homePage()));

    updateWindowTitle(QString());
}

void MainWindow::buildSidebar()
{
    QSettings settings;
    const bool classic = Theme::current() == Theme::ThemeStyle::Classic;
    m_browserSplitter = new QSplitter(Qt::Horizontal, this);
    m_browserSplitter->setChildrenCollapsible(false);
    m_browserSplitter->setHandleWidth(5);
    m_sidebar = new BrowserSidebar(m_tabs, m_browserSplitter);
    m_browserSplitter->addWidget(m_sidebar);
    m_browserSplitter->addWidget(m_tabs);
    m_browserSplitter->setStretchFactor(0, 0);
    m_browserSplitter->setStretchFactor(1, 1);
    connect(m_sidebar, &BrowserSidebar::expandedChanged, this, [this](bool exp) { setSidebarExpanded(exp, true); });
    connect(m_browserSplitter, &QSplitter::splitterMoved, this, [this](int position, int) {
        if (!m_sidebar->isVisible()) return;
        const bool shouldExpand = (position >= 120);
        if (shouldExpand) {
            QSettings().setValue("browser/sidebarWidth", position);
        }
        if (shouldExpand != m_sidebar->isExpanded()) {
            m_sidebar->setExpanded(shouldExpand);
            QSettings().setValue("browser/sidebarExpanded", shouldExpand);
        }
    });
    setCentralWidget(m_browserSplitter);
    // Classic Chrome has one horizontal tab strip and no side panel, so the
    // sidebar starts hidden there regardless of the modern-chrome preference.
    setSidebarVisible(classic ? false
                              : settings.value("browser/sidebarEnabled", true).toBool());
    if (m_sidebar->isVisible()) {
        const bool initExpanded = settings.value("browser/sidebarExpanded", false).toBool();
        m_sidebar->setExpanded(initExpanded);
        setSidebarExpanded(initExpanded, false);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget || widget->window() != this || isMaximized() || isFullScreen())
        return QMainWindow::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton)
            break;
        const Qt::Edges edges = resizeEdgesAt(mouse->globalPosition().toPoint());
        if (edges != Qt::Edges()) {
            m_resizing = true;
            m_resizeEdges = edges;
            m_resizeStartGlobal = mouse->globalPosition().toPoint();
            m_resizeStartGeometry = geometry();
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const QPoint global = mouse->globalPosition().toPoint();
        if (m_resizing && (mouse->buttons() & Qt::LeftButton)) {
            resizeFromGlobalPos(global);
            return true;
        }
        if (m_resizing) {
            m_resizing = false;
            m_resizeEdges = {};
        }
        updateResizeCursor(global);
        break;
    }
    case QEvent::MouseButtonRelease:
        if (m_resizing) {
            m_resizing = false;
            m_resizeEdges = {};
            updateResizeCursor(static_cast<QMouseEvent *>(event)->globalPosition().toPoint());
            return true;
        }
        break;
    default:
        break;
    }
    return QMainWindow::eventFilter(watched, event);
}

Qt::Edges MainWindow::resizeEdgesAt(const QPoint &globalPos) const
{
    constexpr int kResizeBorder = 7;
    const QRect rect = frameGeometry();
    if (!rect.adjusted(-kResizeBorder, -kResizeBorder, kResizeBorder, kResizeBorder).contains(globalPos))
        return {};

    Qt::Edges edges;
    if (globalPos.x() <= rect.left() + kResizeBorder) edges |= Qt::LeftEdge;
    if (globalPos.x() >= rect.right() - kResizeBorder) edges |= Qt::RightEdge;
    if (globalPos.y() <= rect.top() + kResizeBorder) edges |= Qt::TopEdge;
    if (globalPos.y() >= rect.bottom() - kResizeBorder) edges |= Qt::BottomEdge;
    return edges;
}

void MainWindow::updateResizeCursor(const QPoint &globalPos)
{
    const Qt::Edges edges = resizeEdgesAt(globalPos);
    Qt::CursorShape cursor = Qt::ArrowCursor;
    if ((edges.testFlag(Qt::LeftEdge) && edges.testFlag(Qt::TopEdge)) ||
        (edges.testFlag(Qt::RightEdge) && edges.testFlag(Qt::BottomEdge)))
        cursor = Qt::SizeFDiagCursor;
    else if ((edges.testFlag(Qt::RightEdge) && edges.testFlag(Qt::TopEdge)) ||
             (edges.testFlag(Qt::LeftEdge) && edges.testFlag(Qt::BottomEdge)))
        cursor = Qt::SizeBDiagCursor;
    else if (edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge))
        cursor = Qt::SizeHorCursor;
    else if (edges.testFlag(Qt::TopEdge) || edges.testFlag(Qt::BottomEdge))
        cursor = Qt::SizeVerCursor;
    setCursor(cursor);
}

void MainWindow::resizeFromGlobalPos(const QPoint &globalPos)
{
    const QPoint delta = globalPos - m_resizeStartGlobal;
    QRect next = m_resizeStartGeometry;
    const QSize minimum = minimumSizeHint().expandedTo(minimumSize());

    if (m_resizeEdges.testFlag(Qt::LeftEdge))
        next.setLeft(qMin(m_resizeStartGeometry.left() + delta.x(), m_resizeStartGeometry.right() - minimum.width() + 1));
    if (m_resizeEdges.testFlag(Qt::RightEdge))
        next.setRight(qMax(m_resizeStartGeometry.right() + delta.x(), m_resizeStartGeometry.left() + minimum.width() - 1));
    if (m_resizeEdges.testFlag(Qt::TopEdge))
        next.setTop(qMin(m_resizeStartGeometry.top() + delta.y(), m_resizeStartGeometry.bottom() - minimum.height() + 1));
    if (m_resizeEdges.testFlag(Qt::BottomEdge))
        next.setBottom(qMax(m_resizeStartGeometry.bottom() + delta.y(), m_resizeStartGeometry.top() + minimum.height() - 1));
    setGeometry(next);
}

void MainWindow::buildTitleBar()
{
    if (!Theme::metrics().frameless) {
        // Classic chrome: the OS draws the title bar, so the menu row is the
        // topmost thing we own. Using the real QMenuBar (rather than a
        // hand-built container) is what makes the classic build look like a
        // Win32 application instead of a skinned one.
        m_titleBar = nullptr;
        m_menuBar = menuBar();
        return;
    }

    // QMainWindow always puts its menu bar in the topmost row and any
    // TopToolBarArea toolbars in the row(s) below — and will happily pack
    // two toolbars into the *same* row if they fit, which is what was
    // squashing the title bar and the nav toolbar together. Neither
    // behavior is something addToolBar()/menuBar() lets you override, so
    // the title bar can't be one more toolbar: it has to occupy the menu
    // row itself, stacked above a real QMenuBar we build by hand.
    m_titleBar = new TitleBar(this);

    m_menuBar = new QMenuBar(this);

    auto *topContainer = new QWidget(this);
    auto *topLayout = new QVBoxLayout(topContainer);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);
    topLayout->addWidget(m_titleBar);
    topLayout->addWidget(m_menuBar);

    setMenuWidget(topContainer);

    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeRestoreRequested, this, [this]() {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
}

void MainWindow::applyTheme()
{
    qApp->setStyleSheet(ThemeManager::instance().appStyleSheet());
    if (m_addressGlow)
        m_addressGlow->setColor(ThemeManager::instance().accentColor());

    // Re-render the toolbar glyphs for the active chrome. This runs on every
    // accent change and on every interface-style switch, and it has to handle
    // the switch in *both* directions: the classic chrome uses painted icons,
    // the modern chrome uses text arrows, so leaving the classic glyphs behind
    // when going modern would show two different icon languages in one toolbar.
    // Theme::icon() caches per style, so the repaint is not repeated needlessly.
    const bool classic = Theme::current() == Theme::ThemeStyle::Classic;
    if (!m_backAction)
        return; // toolbar not built yet; buildToolbar() applies the style itself

    const bool loading = m_tabs && m_tabs->currentView()
                         && m_tabs->currentView()->page()->isLoading();
    m_backAction->setIcon(classic ? Theme::icon("back") : QIcon());
    m_forwardAction->setIcon(classic ? Theme::icon("forward") : QIcon());
    m_reloadStopAction->setIcon(classic ? Theme::icon(loading ? "stop" : "reload") : QIcon());
    m_reloadStopAction->setText(classic ? QString()
                                        : (loading ? QStringLiteral("\u2715")   // x
                                                   : QStringLiteral("\u27F3"))); // reload
    if (m_homeAction) {
        m_homeAction->setIcon(classic ? Theme::icon("home") : QIcon());
        m_homeAction->setText(classic ? QString() : QStringLiteral("\u2302"));
    }
    if (m_bookmarkStarAction) {
        const bool bookmarked = m_tabs && m_tabs->currentView()
            && m_bookmarkManager->contains(m_tabs->currentView()->url().toString());
        m_bookmarkStarAction->setIcon(classic ? Theme::icon(bookmarked ? "star-filled" : "star")
                                              : QIcon());
        m_bookmarkStarAction->setText(classic ? QString()
                                              : QString::fromUtf8(bookmarked ? "\u2605" : "\u2606"));
    }

    // The two decorations that only exist in the classic omnibox.
    if (m_classicPageIcon)
        m_classicPageIcon->setIcon(Theme::icon("page", 16));
    if (m_classicGoButton)
        m_classicGoButton->setIcon(Theme::icon("go", 16));
    if (m_classicPageIcon)
        m_classicPageIcon->setVisible(classic);
    if (m_classicGoButton)
        m_classicGoButton->setVisible(classic);
}

void MainWindow::buildToolbar()
{
    const bool classic = Theme::current() == Theme::ThemeStyle::Classic;

    auto *toolbar = addToolBar("Navigation");
    m_navToolbar = toolbar;
    toolbar->setObjectName("NavigationToolbar");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(classic ? QSize(16, 16) : QSize(20, 20));
    if (classic)
        toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_backAction = toolbar->addAction(classic ? Theme::icon("back") : QIcon(),
                                      classic ? QString() : QString("\u2190")); // back
    m_backAction->setToolTip("Back");
    connect(m_backAction, &QAction::triggered, this, [this]() {
        if (auto *v = activeView()) v->back();
    });

    m_forwardAction = toolbar->addAction(classic ? Theme::icon("forward") : QIcon(),
                                          classic ? QString() : QString("\u2192")); // forward
    m_forwardAction->setToolTip("Forward");
    connect(m_forwardAction, &QAction::triggered, this, [this]() {
        if (auto *v = activeView()) v->forward();
    });

    m_reloadStopAction = toolbar->addAction(classic ? Theme::icon("reload") : QIcon(),
                                             classic ? QString() : QString("\u27F3")); // reload / stop while loading
    m_reloadStopAction->setToolTip("Reload");
    connect(m_reloadStopAction, &QAction::triggered, this, [this]() {
        if (auto *v = activeView()) {
            if (v->page()->isLoading()) v->stop(); else v->reload();
        }
    });

    // Classic Chrome kept Home, Reload and Stop as separate buttons to the left
    // of the omnibox. The action is always created and only its visibility is
    // the setting: creating it conditionally would leave the Options > Home
    // button toggle with nothing to un-hide later in the session.
    m_homeAction = toolbar->addAction(classic ? Theme::icon("home") : QIcon(),
                                      classic ? QString() : QStringLiteral("\u2302"));
    m_homeAction->setToolTip(tr("Home"));
    connect(m_homeAction, &QAction::triggered, this, [this]() {
        if (auto *v = activeView())
            v->load(QUrl(SettingsDialog(this).homePage()));
    });
    m_homeAction->setVisible(QSettings().value("browser/showHomeButton", true).toBool());
    if (classic)
        toolbar->addSeparator();

    m_addressBar = new QLineEdit(toolbar);
    // The object name is what the classic stylesheet targets for its 23 px,
    // square-cornered, page-icon-inset omnibox.
    m_addressBar->setObjectName(classic ? "ClassicOmnibox" : "Omnibox");
    m_addressBar->setPlaceholderText(classic ? QString() : QString("Search or enter address"));
    m_addressBar->setClearButtonEnabled(!classic);
    connect(m_addressBar, &QLineEdit::returnPressed, this, &MainWindow::navigateToAddressBarInput);
    toolbar->addWidget(m_addressBar);
    if (classic)
        buildClassicOmniboxDecor();

    // A restrained "breathing" accent around the address bar makes page
    // loading visible even when the progress bar is near 0% or hidden. It is
    // only ever enabled during a load, and it is skipped entirely on the
    // classic chrome (Chrome 2009 had no such thing) and on the Legacy CPU
    // tier, where any graphics effect goes through the software compositor.
    if (!classic && PerformanceProfile::animationsEnabled()) {
        m_addressGlow = new QGraphicsDropShadowEffect(m_addressBar);
        m_addressGlow->setOffset(0, 0);
        m_addressGlow->setColor(ThemeManager::instance().accentColor());
        m_addressGlow->setBlurRadius(0);
        m_addressGlow->setEnabled(false);
        m_addressBar->setGraphicsEffect(m_addressGlow);

        m_addressGlowAnim = new QPropertyAnimation(m_addressGlow, "blurRadius", this);
        m_addressGlowAnim->setDuration(950);
        m_addressGlowAnim->setKeyValueAt(0.0, 2.0);
        m_addressGlowAnim->setKeyValueAt(0.5, 16.0);
        m_addressGlowAnim->setKeyValueAt(1.0, 2.0);
        m_addressGlowAnim->setLoopCount(-1);
        m_addressGlowAnim->setEasingCurve(QEasingCurve::InOutSine);
    }

    m_bookmarkStarAction = toolbar->addAction(classic ? Theme::icon("star") : QIcon(),
                                              classic ? QString() : QString("\u2606")); // outline star; filled when bookmarked
    m_bookmarkStarAction->setToolTip("Bookmark this page");
    connect(m_bookmarkStarAction, &QAction::triggered, this, &MainWindow::toggleBookmarkForCurrentPage);

    auto *readerAction = toolbar->addAction(classic ? Theme::icon("reader") : QIcon(),
                                            classic ? QString() : QString("\u2637")); // reader/article glyph
    readerAction->setToolTip("Reader mode");
    connect(readerAction, &QAction::triggered, this, [this]() {
        auto *v = activeView();
        if (!v)
            return;
        v->page()->runJavaScript(ReaderMode::toggleScript(), [this](const QVariant &result) {
            if (result.toString() == "no-article")
                statusBar()->showMessage("Reader mode isn't available for this page", 3000);
        });
    });

    // Classic Chrome showed loading as a green progress sliver over the
    // status bar rather than a spinner next to the omnibox.
    if (!classic) {
        m_spinner = new SpinnerWidget(toolbar);
        toolbar->addWidget(m_spinner);
    }

    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(120);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);
    if (!classic)
        toolbar->addWidget(m_progressBar);

    m_progressAnim = new QPropertyAnimation(m_progressBar, "value", this);
    m_progressAnim->setDuration(PerformanceProfile::animationsEnabled() ? 180 : 0);
    m_progressAnim->setEasingCurve(QEasingCurve::OutCubic);

    if (classic && statusBar()) {
        // Status-bar progress, as in the 2009 chrome.
        m_progressBar->setMaximumWidth(140);
        m_progressBar->setFixedHeight(10);
        statusBar()->addPermanentWidget(m_progressBar, 0);
    }

    // The AI sidebar and split view are modern-chrome features: Chrome 2009
    // had neither, so the classic toolbar stays true to its reference and
    // leaves them out (they remain reachable from the View menu).
    if (!classic) {
        // AI Sidebar toggle button (Ctrl+Shift+A)
        m_aiSidebarAction = toolbar->addAction("AI"); // AI label for sidebar toggle
        m_aiSidebarAction->setToolTip("AI Sidebar (Ctrl+Shift+A)");
        m_aiSidebarAction->setCheckable(true);
        m_aiSidebarAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
        connect(m_aiSidebarAction, &QAction::triggered, this, [this](bool checked) {
            if (m_aiSidebar)
                m_aiSidebar->setVisible(checked);
        });

        // SplitView toggle button
        m_splitViewAction = toolbar->addAction("\u29C9"); // split view icon
        m_splitViewAction->setToolTip("Toggle Split View");
        m_splitViewAction->setCheckable(true);
        connect(m_splitViewAction, &QAction::triggered, this, [this](bool checked) {
            toggleSplitView(checked);
        });
    }

    updateNavigationState();
}

void MainWindow::buildBookmarksBar()
{
    // addToolBarBreak forces the next toolbar onto a new row instead of
    // letting QMainWindow pack it into the same row as the nav toolbar —
    // the same trick buildTitleBar()'s comment explains for the title bar.
    addToolBarBreak(Qt::TopToolBarArea);

    auto *toolbar = addToolBar("Bookmarks");
    m_bookmarkToolbar = toolbar;
    toolbar->setObjectName("BookmarkToolbar");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->toggleViewAction()->setVisible(false); // don't let it show up in a right-click "toggle toolbar" menu
    toolbar->setContentsMargins(0, 0, 0, 0);

    m_bookmarkBar = new BookmarkBar(m_bookmarkManager, toolbar);
    connect(m_bookmarkBar, &BookmarkBar::bookmarkActivated, this, [this](const QUrl &url) {
        if (auto *v = activeView())
            v->load(url);
    });
    toolbar->addWidget(m_bookmarkBar);
}

void MainWindow::toggleBookmarkForCurrentPage()
{
    auto *v = activeView();
    if (!v)
        return;

    const QString url = v->url().toString();
    const int idx = m_bookmarkManager->indexOfUrl(url);
    if (idx != -1)
        m_bookmarkManager->removeAt(idx);
    else
        m_bookmarkManager->add(v->title(), url);

    updateBookmarkStar(v->url());
}

void MainWindow::updateBookmarkStar(const QUrl &url)
{
    const bool bookmarked = m_bookmarkManager->contains(url.toString());
    if (Theme::current() == Theme::ThemeStyle::Classic) {
        // The classic chrome uses painted glyphs, so the filled/outline state
        // is expressed by swapping the icon rather than the button text.
        m_bookmarkStarAction->setIcon(Theme::icon(bookmarked ? "star-filled" : "star"));
        m_bookmarkStarAction->setText(QString());
    } else {
        m_bookmarkStarAction->setIcon(QIcon());
        m_bookmarkStarAction->setText(bookmarked ? "\u2605" : "\u2606");
    }
    m_bookmarkStarAction->setToolTip(bookmarked ? "Remove bookmark" : "Bookmark this page");
}

void MainWindow::buildMenu()
{
    if (Theme::current() == Theme::ThemeStyle::Classic) {
        buildClassicMenu();
        return;
    }
    buildModernMenu();
}

void MainWindow::announceStyleChange(Theme::ThemeStyle style)
{
    // The restart entry differs per chrome (Tools > Restart on classic, Menu >
    // Restart on modern), so name the one the user can actually see.
    const bool classic = Theme::current() == Theme::ThemeStyle::Classic;
    const QString where = classic ? tr("Tools \u25b8 Restart") : tr("Menu \u25b8 Restart");
    QMessageBox::information(
        this, tr("Interface Style"),
        tr("The %1 interface will be used the next time Cloudar Browser starts. "
           "Use %2 to switch now.")
            .arg(style == Theme::ThemeStyle::Classic ? tr("classic") : tr("modern"), where));
}

void MainWindow::buildModernMenu()
{
    auto *menu = m_menuBar->addMenu("&Menu");

    auto *newTab = menu->addAction("New Tab");
    newTab->setShortcut(QKeySequence::AddTab);
    connect(newTab, &QAction::triggered, this, [this]() { m_tabs->addNewTab(); });

    auto *newIncognito = menu->addAction("New Incognito Window");
    newIncognito->setShortcut(QKeySequence("Ctrl+Shift+N"));
    connect(newIncognito, &QAction::triggered, this, [this]() {
        auto *incognitoWindow = new MainWindow(true);
        incognitoWindow->show();
    });

    m_sidebarAction = menu->addAction("Show Sidebar");
    m_sidebarAction->setCheckable(true);
    m_sidebarAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    m_sidebarAction->setChecked(m_sidebar && m_sidebar->isVisible());
    connect(m_sidebarAction, &QAction::toggled, this, &MainWindow::setSidebarVisible);

    auto *downloads = menu->addAction("Downloads");
    connect(downloads, &QAction::triggered, this, [this]() { m_downloadManager->showDialog(); });

    auto *history = menu->addAction("History");
    connect(history, &QAction::triggered, this, &MainWindow::showHistoryDialog);

    // One shared handler for both chromes so the two menus cannot drift apart.
    auto *themeMenu = menu->addMenu(tr("Interface Style"));
    m_themeClassicAction = themeMenu->addAction(tr("Classic (Chrome 2009)"));
    m_themeModernAction = themeMenu->addAction(tr("Modern (dark)"));
    auto *styleGroup = new QActionGroup(this);
    for (QAction *action : {m_themeClassicAction, m_themeModernAction}) {
        action->setCheckable(true);
        styleGroup->addAction(action);
    }
    // Checked state follows the *saved* preference, not the running style: a
    // --modern launch must not make this menu claim the user chose modern.
    m_themeModernAction->setChecked(Theme::savedStyle() == Theme::ThemeStyle::Modern);
    m_themeClassicAction->setChecked(!m_themeModernAction->isChecked());

    auto applyStyleChoice = [this](Theme::ThemeStyle style) {
        // Saved only: the window model and menu bar cannot change in place.
        Theme::setCurrent(style);
        announceStyleChange(style);
    };
    connect(m_themeClassicAction, &QAction::triggered, this, [applyStyleChoice]() {
        applyStyleChoice(Theme::ThemeStyle::Classic);
    });
    connect(m_themeModernAction, &QAction::triggered, this, [applyStyleChoice]() {
        applyStyleChoice(Theme::ThemeStyle::Modern);
    });

    auto *find = menu->addAction("Find in page");
    find->setShortcut(QKeySequence::Find);
    connect(find, &QAction::triggered, this, &MainWindow::showFindBar);

    auto *zoom = menu->addMenu("Zoom");
    m_zoomInAction = zoom->addAction("Zoom In");
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this, [this](){ if (auto *v = activeView()) setZoom(v->zoomFactor() + .1); });
    m_zoomOutAction = zoom->addAction("Zoom Out");
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, [this](){ if (auto *v = activeView()) setZoom(v->zoomFactor() - .1); });
    m_zoomResetAction = zoom->addAction("Actual Size");
    m_zoomResetAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(m_zoomResetAction, &QAction::triggered, this, [this](){ setZoom(1.0); });

    auto *toggleBookmarksBar = menu->addAction("Show Bookmarks Bar");
    toggleBookmarksBar->setCheckable(true);
    toggleBookmarksBar->setChecked(QSettings().value("browser/bookmarkBarVisible", true).toBool());
    toggleBookmarksBar->setShortcut(QKeySequence("Ctrl+Shift+B"));
    connect(toggleBookmarksBar, &QAction::toggled, this, [this](bool on) {
        if (m_bookmarkToolbar) {
            // Ctrl+Shift+B is a window-level shortcut, so it applies for this
            // session only; the saved preference is the Settings switch.
            m_bookmarkToolbar->setVisible(on);
        }
    });

    auto *extensions = menu->addAction("Extensions");
    connect(extensions, &QAction::triggered, this, &MainWindow::showExtensionsDialog);

    // Split view was only reachable from the toolbar button, which does not
    // exist on the classic chrome — the feature was there, with no way to ask
    // for it. One action, shared by both chromes' menus and the toolbar.
    menu->addSeparator();
    m_splitViewMenuAction = menu->addAction(tr("Split View"));
    m_splitViewMenuAction->setCheckable(true);
    m_splitViewMenuAction->setShortcut(QKeySequence("Ctrl+Shift+D"));
    connect(m_splitViewMenuAction, &QAction::toggled, this, &MainWindow::toggleSplitView);

    menu->addSeparator();

    auto *settings = menu->addAction("Settings");
    connect(settings, &QAction::triggered, this, &MainWindow::openSettingsPage);

    // The interface-style handler tells the user to "Use Tools ▸ Restart to
    // switch now", but the modern menu is a single Menu and never had that
    // entry — so the advertised escape hatch did not exist and picking a
    // different chrome looked like it had done nothing at all.
    auto *restart = menu->addAction(tr("Restart"));
    restart->setShortcut(QKeySequence("Ctrl+Shift+R"));
    connect(restart, &QAction::triggered, this, [this]() {
        if (m_settingsBridge)
            m_settingsBridge->relaunch();
    });

    auto *quit = menu->addAction("Quit");
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::setSidebarVisible(bool visible)
{
    if (!m_sidebar || !m_browserSplitter)
        return;
    // The classic chrome has exactly one tab surface, so a hidden side panel
    // must not leave the browser without a visible tab strip to switch tabs.
    const bool classic = Theme::current() == Theme::ThemeStyle::Classic;
    if (classic && !visible)
        m_tabs->setHorizontalTabBarVisible(true);

    m_sidebar->setVisible(visible);
    m_browserSplitter->handle(1)->setVisible(visible);
    if (!classic || visible)
        m_tabs->setHorizontalTabBarVisible(!visible);
    if (m_sidebarAction && m_sidebarAction->isChecked() != visible) {
        m_sidebarAction->blockSignals(true);
        m_sidebarAction->setChecked(visible);
        m_sidebarAction->blockSignals(false);
    }
    // Only a user-driven toggle (the View menu, the settings page) should be
    // persisted. The classic chrome force-hides the panel at startup, and that
    // must not overwrite the preference the modern chrome will want back.
    if (visible || Theme::current() != Theme::ThemeStyle::Classic || m_firstShowDone)
        QSettings().setValue("browser/sidebarEnabled", visible);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (!m_firstShowDone) {
        m_firstShowDone = true;
        QTimer::singleShot(0, this, [this]() {
            if (m_sidebar && m_sidebar->isVisible() && m_browserSplitter) {
                const bool expanded = m_sidebar->isExpanded();
                const int target = expanded ? QSettings().value("browser/sidebarWidth", 240).toInt() : 54;
                const int totalWidth = m_browserSplitter->width();
                const int rightWidth = (totalWidth > target) ? (totalWidth - target) : 10000;
                m_browserSplitter->setSizes({target, rightWidth});
            }
        });
    }
}

void MainWindow::setSidebarExpanded(bool expanded, bool animated)
{
    if (!m_sidebar || !m_browserSplitter)
        return;
    const int target = expanded ? QSettings().value("browser/sidebarWidth", 240).toInt() : 54;
    QSettings().setValue("browser/sidebarExpanded", expanded);

    const int totalWidth = m_browserSplitter->width();
    const int rightWidth = (totalWidth > target) ? (totalWidth - target) : 10000;

    if (!animated || !isVisible()) {
        m_browserSplitter->setSizes({target, rightWidth});
        return;
    }

    QList<int> sizes = m_browserSplitter->sizes();
    int currentWidth = sizes.isEmpty() ? 54 : sizes[0];
    if (currentWidth == target) {
        m_browserSplitter->setSizes({target, rightWidth});
        return;
    }

    if (m_sidebarAnimation) {
        m_sidebarAnimation->stop();
        delete m_sidebarAnimation;
        m_sidebarAnimation = nullptr;
    }

    m_sidebarAnimation = new QVariantAnimation(this);
    m_sidebarAnimation->setDuration(220);
    m_sidebarAnimation->setStartValue(currentWidth);
    m_sidebarAnimation->setEndValue(target);
    m_sidebarAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_sidebarAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
        int w = val.toInt();
        int total = m_browserSplitter->width();
        m_browserSplitter->setSizes({w, qMax(1, total - w)});
    });
    connect(m_sidebarAnimation, &QVariantAnimation::finished, this, [this]() {
        m_sidebarAnimation->deleteLater();
        m_sidebarAnimation = nullptr;
    });
    m_sidebarAnimation->start();
}

void MainWindow::applySettings()
{
    SettingsDialog dlg(this); // used here only to read persisted values
    m_profile->setPersistentCookiesPolicy(
        dlg.persistCookies() ? QWebEngineProfile::AllowPersistentCookies
                              : QWebEngineProfile::NoPersistentCookies);
}

void MainWindow::updateNavigationState()
{
    auto *view = m_tabs ? m_tabs->currentView() : nullptr;
    const bool hasView = view != nullptr;
    if (m_backAction) m_backAction->setEnabled(hasView && view->history()->canGoBack());
    if (m_forwardAction) m_forwardAction->setEnabled(hasView && view->history()->canGoForward());
    if (m_reloadStopAction) {
        const bool loading = hasView && view->page()->isLoading();
        if (Theme::current() == Theme::ThemeStyle::Classic) {
            m_reloadStopAction->setIcon(Theme::icon(loading ? "stop" : "reload"));
            m_reloadStopAction->setText(QString());
        } else {
            // Modern chrome: a text glyph, so any icon from a previous classic
            // session must be cleared or both are drawn side by side.
            m_reloadStopAction->setIcon(QIcon());
            m_reloadStopAction->setText(loading ? "\u2715" : "\u27F3");
        }
        m_reloadStopAction->setToolTip(loading ? "Stop" : "Reload");
    }
}

void MainWindow::showFindBar()
{
    auto *view = activeView();
    if (!view) return;
    bool accepted = false;
    const QString text = QInputDialog::getText(this, "Find in page", "Find:", QLineEdit::Normal, {}, &accepted);
    if (accepted && !text.isEmpty())
        view->findText(text);
}

void MainWindow::setZoom(qreal zoom)
{
    if (auto *view = activeView()) {
        view->setZoomFactor(qBound<qreal>(0.25, zoom, 5.0));
        statusBar()->showMessage(QString("Zoom: %1%").arg(qRound(view->zoomFactor() * 100)), 1500);
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
QString MainWindow::permissionName(QWebEnginePermission::PermissionType type) const
{
    using Type = QWebEnginePermission::PermissionType;
    switch (type) {
    case Type::MediaAudioCapture: return "microphone";
    case Type::MediaVideoCapture: return "camera";
    case Type::MediaAudioVideoCapture: return "camera and microphone";
    case Type::Geolocation: return "location";
    case Type::Notifications: return "notifications";
    case Type::MouseLock: return "pointer lock";
    case Type::DesktopVideoCapture: return "screen sharing";
    case Type::DesktopAudioVideoCapture: return "screen and audio sharing";
    case Type::ClipboardReadWrite: return "clipboard";
    case Type::LocalFontsAccess: return "local fonts";
    default: return "this capability";
    }
}

void MainWindow::requestPermission(const QWebEnginePermission &permission)
{
    if (!permission.isValid()) return;
    const QUrl origin = permission.origin();
    // Decisions are keyed by capability name, not by the engine's numeric enum:
    // the numbers differ between Qt 6.2 and 6.7+, and the cloudar://permissions
    // page has to be able to read and edit exactly what this code writes.
    const QString capability = PermissionStore::capabilityForEngineType(int(permission.permissionType()));
    const QString saved = capability.isEmpty()
        ? QString()
        : PermissionStore::decisionFor(origin.host(), capability);
    if (saved == "allow") {
        permission.grant();
        return;
    }
    if (saved == "deny") {
        permission.deny();
        return;
    }
    {
        QMessageBox question(this);
        question.setWindowTitle("Permission request");
        question.setText(QString("%1 wants to use your %2.").arg(origin.host(), permissionName(permission.permissionType())));
        question.setInformativeText("Choose Allow only for sites you trust.");
        auto *deny = question.addButton("Deny", QMessageBox::RejectRole);
        auto *allow = question.addButton("Allow", QMessageBox::AcceptRole);
        question.exec();
        const bool granted = question.clickedButton() == allow;
        Q_UNUSED(deny);
        // Editable afterwards in cloudar://permissions, which is why the
        // capability has to be a name: that page has no way to know which Qt
        // version's enum produced a number.
        if (!capability.isEmpty())
            PermissionStore::setDecision(origin.host(), capability, granted ? "allow" : "deny");
        if (granted) permission.grant(); else permission.deny();
    }
}
#else
QString MainWindow::featureName(QWebEnginePage::Feature feature) const
{
    switch (feature) {
    case QWebEnginePage::Notifications: return "notifications";
    case QWebEnginePage::Geolocation: return "location";
    case QWebEnginePage::MediaAudioCapture: return "microphone";
    case QWebEnginePage::MediaVideoCapture: return "camera";
    case QWebEnginePage::MediaAudioVideoCapture: return "camera and microphone";
    case QWebEnginePage::MouseLock: return "pointer lock";
    case QWebEnginePage::DesktopVideoCapture: return "screen sharing";
    case QWebEnginePage::DesktopAudioVideoCapture: return "screen and audio sharing";
    default: return "this capability";
    }
}

void MainWindow::handleFeaturePermission(const QUrl &origin, QWebEnginePage::Feature feature, QWebEnginePage *page)
{
    if (!page) return;
    const QString capability = PermissionStore::capabilityForEngineType(int(feature));
    const QString saved = capability.isEmpty()
        ? QString()
        : PermissionStore::decisionFor(origin.host(), capability);
    if (saved == "allow") {
        page->setFeaturePermission(origin, feature, QWebEnginePage::PermissionGrantedByUser);
        return;
    }
    if (saved == "deny") {
        page->setFeaturePermission(origin, feature, QWebEnginePage::PermissionDeniedByUser);
        return;
    }
    {
        QMessageBox question(this);
        question.setWindowTitle("Permission request");
        question.setText(QString("%1 wants to use your %2.").arg(origin.host(), featureName(feature)));
        question.setInformativeText("Choose Allow only for sites you trust.");
        auto *deny = question.addButton("Deny", QMessageBox::RejectRole);
        auto *allow = question.addButton("Allow", QMessageBox::AcceptRole);
        question.exec();
        const bool granted = question.clickedButton() == allow;
        Q_UNUSED(deny);
        if (!capability.isEmpty())
            PermissionStore::setDecision(origin.host(), capability, granted ? "allow" : "deny");
        page->setFeaturePermission(origin, feature,
            granted ? QWebEnginePage::PermissionGrantedByUser : QWebEnginePage::PermissionDeniedByUser);
    }
}
#endif

void MainWindow::saveSession()
{
    if (m_isIncognito)
        return;
    if (m_sessionManager && m_tabs)
        m_sessionManager->save(m_tabs->openUrls(), m_tabs->currentIndex());
}

void MainWindow::openUrl(const QUrl &url)
{
    if (m_tabs->count() == 0)
        m_tabs->addNewTab(url);
    else if (auto *v = activeView())
        v->load(url);
}

void MainWindow::navigateToAddressBarInput()
{
    const QString text = m_addressBar->text().trimmed();
    if (text.isEmpty())
        return;

    QUrl url = QUrl::fromUserInput(text);
    // Preserve explicit schemes (including file:// and browser://) even
    // when they don't contain a dot; otherwise a local file or internal
    // page would accidentally become a search query.
    const bool looksLikeUrl = !text.contains(' ') &&
        (text.contains("://") || text.startsWith("about:") || text.contains('.') ||
         text == "localhost" || text.startsWith("localhost:"));

    if (!looksLikeUrl || !url.isValid()) {
        SettingsDialog dlg(this);
        QString tmpl = dlg.searchEngineTemplate();
        url = QUrl(tmpl.replace("%s", QUrl::toPercentEncoding(text)));
    }

    if (auto *v = activeView()) {
        v->load(url);
        if (!m_isIncognito && m_historyManager)
            m_historyManager->addVisit(text, url.toString());
    }
}

void MainWindow::updateAddressBar(const QUrl &url)
{
    m_addressBar->setText(url.toString());
    m_addressBar->setCursorPosition(0);
}

void MainWindow::updateWindowTitle(const QString &title)
{
    const QString prefix = m_isIncognito ? "🕶 Incognito - " : "";
    const QString full = title.isEmpty() ? (prefix + "Cloudar Browser") : (prefix + title + " - Cloudar Browser");
    setWindowTitle(full);
    if (m_titleBar)
        m_titleBar->setTitle(full);
    if (!m_isIncognito && m_historyManager) {
        if (auto *v = m_tabs->currentView())
            m_historyManager->addVisit(title, v->url().toString());
    }
}

void MainWindow::updateProgress(int percent)
{
    if (Theme::current() == Theme::ThemeStyle::Classic) {
        // Chrome 2009 had no progress control in the toolbar; it drew a thin
        // green bar across the status bar. A determinate QProgressBar added
        // permanently to the status bar is the closest faithful equivalent.
        if (!m_progressBar)
            return;
        if (percent <= 0) {
            m_progressBar->setVisible(false);
            return;
        }
        if (!m_progressBar->isVisible() && statusBar()) {
            if (!statusBar()->isVisible())
                statusBar()->show();
        }
        m_progressBar->setVisible(true);
        m_progressBar->setValue(percent);
        if (percent >= 100)
            QTimer::singleShot(400, m_progressBar, [this]() { m_progressBar->setVisible(false); });
        return;
    }

    if (percent <= 0) {
        m_progressBar->setVisible(false);
        return;
    }

    m_progressBar->setVisible(true);
    m_progressAnim->stop();
    m_progressAnim->setStartValue(m_progressBar->value());
    m_progressAnim->setEndValue(percent);
    m_progressAnim->start();

    if (percent >= 100) {
        // Let the bar visibly reach 100% before hiding, instead of
        // vanishing mid-animation.
        connect(m_progressAnim, &QPropertyAnimation::finished, this, [this]() {
            m_progressBar->setVisible(false);
        }, Qt::SingleShotConnection);
    }
}

void MainWindow::showHistoryDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("History");
    dialog.resize(480, 400);

    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QListWidget(&dialog);
    for (const auto &entry : m_historyManager->entries()) {
        list->addItem(QString("%1  —  %2")
                          .arg(entry.title.isEmpty() ? entry.url : entry.title, entry.url));
    }
    layout->addWidget(list);

    connect(list, &QListWidget::itemDoubleClicked, this, [this, &dialog](QListWidgetItem *item) {
        const QString text = item->text();
        const QString url = text.section("—", 1).trimmed();
        if (!url.isEmpty()) {
            m_tabs->addNewTab(QUrl(url));
            dialog.accept();
        }
    });

    auto *clearBtn = new QPushButton("Clear History", &dialog);
    connect(clearBtn, &QPushButton::clicked, this, [this, list]() {
        m_historyManager->clear();
        list->clear();
    });
    layout->addWidget(clearBtn);

    dialog.exec();
}

void MainWindow::openSettingsPage()
{
    // A real cloudar:// page now (see resources/settings.html + SettingsBridge)
    // rather than a modal dialog: it applies most changes live (accent
    // color, cookie policy) and reads/writes the same QSettings values the
    // rest of the app already relies on. Reuses an existing Settings tab
    // instead of piling up duplicates.
    m_tabs->focusOrOpenTab("cloudar://settings", QUrl("cloudar://settings/"));
}

void MainWindow::showExtensionsDialog()
{
    ExtensionsDialog dlg(m_extensionManager, m_webExtensionManager, this);
    dlg.exec();
}

// ---------------------------------------------------------------------------
// Classic chrome (Chrome 0.2 / 1.0, 2009)
// ---------------------------------------------------------------------------

void MainWindow::buildClassicMenu()
{
    // Chrome 2009 exposed its actions through a single "Control the current
    // page" button plus a small wrench menu; this is the same content laid out
    // as a conventional Win32 menu bar, which is what users of that era
    // expect from a Windows application.
    auto *fileMenu = m_menuBar->addMenu(tr("&File"));

    auto *newTab = fileMenu->addAction(tr("New &tab"));
    newTab->setShortcut(QKeySequence::AddTab);
    connect(newTab, &QAction::triggered, this, [this]() { m_tabs->addNewTab(); });

    auto *newWindow = fileMenu->addAction(tr("New &window"));
    newWindow->setShortcut(QKeySequence("Ctrl+N"));
    connect(newWindow, &QAction::triggered, this, [this]() {
        auto *window = new MainWindow(false);
        window->show();
    });

    auto *newIncognito = fileMenu->addAction(tr("New &incognito window"));
    newIncognito->setShortcut(QKeySequence("Ctrl+Shift+N"));
    connect(newIncognito, &QAction::triggered, this, [this]() {
        auto *window = new MainWindow(true);
        window->show();
    });

    fileMenu->addSeparator();

    auto *openLocation = fileMenu->addAction(tr("Open &location"));
    openLocation->setShortcut(QKeySequence("Ctrl+L"));
    connect(openLocation, &QAction::triggered, this, [this]() {
        m_addressBar->setFocus();
        m_addressBar->selectAll();
    });

    auto *closeTab = fileMenu->addAction(tr("&Close tab"));
    closeTab->setShortcut(QKeySequence::Close);
    connect(closeTab, &QAction::triggered, this, [this]() {
        m_tabs->closeTabAt(m_tabs->currentIndex());
    });

    fileMenu->addSeparator();
    auto *quit = fileMenu->addAction(tr("E&xit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    auto *editMenu = m_menuBar->addMenu(tr("&Edit"));
    // Shortcut only, no stored pointer: nothing outside this menu needs to
    // enable/disable it, and a member that is never read is just another
    // pointer that can dangle.
    auto *find = editMenu->addAction(tr("&Find in page..."));
    find->setShortcut(QKeySequence::Find);
    connect(find, &QAction::triggered, this, &MainWindow::showFindBar);

    auto *viewMenu = m_menuBar->addMenu(tr("&View"));
    m_zoomInAction = viewMenu->addAction(tr("Zoom &in"));
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this,
            [this]() { if (auto *v = activeView()) setZoom(v->zoomFactor() + .1); });
    m_zoomOutAction = viewMenu->addAction(tr("Zoom &out"));
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this,
            [this]() { if (auto *v = activeView()) setZoom(v->zoomFactor() - .1); });
    m_zoomResetAction = viewMenu->addAction(tr("&Normal size"));
    m_zoomResetAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(m_zoomResetAction, &QAction::triggered, this, [this]() { setZoom(1.0); });

    viewMenu->addSeparator();
    auto *alwaysShowBookmarks = viewMenu->addAction(tr("Always show &bookmarks bar"));
    alwaysShowBookmarks->setCheckable(true);
    alwaysShowBookmarks->setChecked(QSettings().value("browser/bookmarkBarVisible", true).toBool());
    connect(alwaysShowBookmarks, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue("browser/bookmarkBarVisible", on);
        if (m_bookmarkToolbar)
            m_bookmarkToolbar->setVisible(on);
    });
    if (m_bookmarkToolbar)
        m_bookmarkToolbar->setVisible(alwaysShowBookmarks->isChecked());

    m_sidebarAction = viewMenu->addAction(tr("Show &side panel"));
    m_sidebarAction->setCheckable(true);
    m_sidebarAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    m_sidebarAction->setChecked(m_sidebar && m_sidebar->isVisible());
    connect(m_sidebarAction, &QAction::toggled, this, &MainWindow::setSidebarVisible);

    // Modern-chrome features stay reachable from a menu even when their
    // toolbar buttons are hidden for period accuracy.
    viewMenu->addSeparator();
    m_splitViewMenuAction = viewMenu->addAction(tr("Sp&lit view"));
    auto *splitAction = m_splitViewMenuAction;
    splitAction->setCheckable(true);
    splitAction->setShortcut(QKeySequence("Ctrl+Shift+D"));
    // toggled (not triggered) so the lambda signature matches the bool-taking
    // slot and the menu check state always mirrors the actual view.
    connect(splitAction, &QAction::toggled, this, &MainWindow::toggleSplitView);

    auto *aiAction = viewMenu->addAction(tr("AI assistant panel"));
    aiAction->setCheckable(true);
    aiAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
    connect(aiAction, &QAction::triggered, this, [this](bool on) {
        if (m_aiSidebar) {
            m_aiSidebar->setVisible(on);
            if (m_aiSidebarAction)
                m_aiSidebarAction->setChecked(on);
        }
    });

    auto *historyMenu = m_menuBar->addMenu(tr("&History"));
    auto *history = historyMenu->addAction(tr("&History..."));
    history->setShortcut(QKeySequence("Ctrl+H"));
    connect(history, &QAction::triggered, this, &MainWindow::showHistoryDialog);
    auto *bookmarks = historyMenu->addAction(tr("&Bookmark manager"));
    bookmarks->setShortcut(QKeySequence("Ctrl+B"));
    connect(bookmarks, &QAction::triggered, this,
            [this]() { m_tabs->focusOrOpenTab("cloudar://bookmarks", QUrl("cloudar://bookmarks/")); });
    auto *downloads = historyMenu->addAction(tr("&Downloads"));
    downloads->setShortcut(QKeySequence("Ctrl+J"));
    connect(downloads, &QAction::triggered, this, [this]() { m_downloadManager->showDialog(); });

    auto *toolsMenu = m_menuBar->addMenu(tr("&Tools"));
    auto *extensions = toolsMenu->addAction(tr("&Extensions"));
    connect(extensions, &QAction::triggered, this, &MainWindow::showExtensionsDialog);
    auto *settings = toolsMenu->addAction(tr("&Options..."));
    connect(settings, &QAction::triggered, this, &MainWindow::openSettingsPage);
    auto *flags = toolsMenu->addAction(tr("&Under the hood"));
    connect(flags, &QAction::triggered, this,
            [this]() { m_tabs->focusOrOpenTab("cloudar://flags", QUrl("cloudar://flags/")); });
    auto *restart = toolsMenu->addAction(tr("&Restart"));
    connect(restart, &QAction::triggered, this, [this]() {
        // Settings that change the Chromium command line only take effect on a
        // new process, so a restart has to be one click away.
        if (m_settingsBridge)
            m_settingsBridge->relaunch();
    });

    toolsMenu->addSeparator();
    auto *themeMenu = toolsMenu->addMenu(tr("&Appearance"));
    m_themeClassicAction = themeMenu->addAction(tr("Classic (Chrome 2009)"));
    m_themeModernAction = themeMenu->addAction(tr("Modern (dark)"));
    auto *styleGroup = new QActionGroup(this);
    for (QAction *action : {m_themeClassicAction, m_themeModernAction}) {
        action->setCheckable(true);
        styleGroup->addAction(action);
    }
    m_themeModernAction->setChecked(Theme::savedStyle() == Theme::ThemeStyle::Modern);
    m_themeClassicAction->setChecked(!m_themeModernAction->isChecked());

    auto applyStyleChoice = [this](Theme::ThemeStyle style) {
        Theme::setCurrent(style);
        // Window model, tab placement and menu contents all differ between the
        // two chromes, so the honest answer is that the change lands on the next
        // launch (or immediately after the restart entry in the menu).
        announceStyleChange(style);
    };
    connect(m_themeClassicAction, &QAction::triggered, this, [applyStyleChoice]() {
        applyStyleChoice(Theme::ThemeStyle::Classic);
    });
    connect(m_themeModernAction, &QAction::triggered, this, [applyStyleChoice]() {
        applyStyleChoice(Theme::ThemeStyle::Modern);
    });

    auto *helpMenu = m_menuBar->addMenu(tr("&Help"));
    auto *about = helpMenu->addAction(tr("&About Cloudar Browser"));
    connect(about, &QAction::triggered, this,
            [this]() { m_tabs->focusOrOpenTab("cloudar://about", QUrl("cloudar://about/")); });
}

void MainWindow::buildClassicOmniboxDecor()
{
    if (!m_addressBar)
        return;

    // Chrome 2009 put a small page/favicon glyph at the left inside edge of the
    // address field and a green "go" arrow at the right edge when the field
    // had focus. Both are child widgets of the QLineEdit so they scroll with
    // it, and the stylesheet reserves the matching padding.
    m_classicPageIcon = new QToolButton(m_addressBar);
    m_classicPageIcon->setObjectName("ClassicPageIcon");
    m_classicPageIcon->setIcon(Theme::icon("page", 16));
    m_classicPageIcon->setIconSize(QSize(16, 16));
    m_classicPageIcon->setFixedSize(18, 18);
    m_classicPageIcon->setAutoRaise(true);
    m_classicPageIcon->setFocusPolicy(Qt::NoFocus);
    m_classicPageIcon->setCursor(Qt::ArrowCursor);
    m_classicPageIcon->move(3, 2);
    m_classicPageIcon->show();

    m_classicGoButton = new QToolButton(m_addressBar);
    m_classicGoButton->setObjectName("ClassicOmniboxGo");
    m_classicGoButton->setIcon(Theme::icon("go", 16));
    m_classicGoButton->setIconSize(QSize(14, 14));
    m_classicGoButton->setFixedSize(18, 18);
    m_classicGoButton->setAutoRaise(true);
    m_classicGoButton->setFocusPolicy(Qt::NoFocus);
    m_classicGoButton->setToolTip(tr("Go"));
    connect(m_classicGoButton, &QToolButton::clicked,
            this, &MainWindow::navigateToAddressBarInput);
    m_classicGoButton->hide();

    // "Go appears when you type" is exactly how the 2009 omnibox behaved.
    // Both decorations are children of the omnibox and survive a live switch to
    // the modern chrome, so every update re-checks the active style instead of
    // assuming classic for the rest of the process.
    connect(m_addressBar, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (!m_classicGoButton)
            return;
        const bool classic = Theme::current() == Theme::ThemeStyle::Classic;
        m_classicGoButton->setVisible(classic && !text.isEmpty());
        m_classicGoButton->move(m_addressBar->width() - 22, 2);
    });
    connect(m_addressBar, &QLineEdit::cursorPositionChanged, this, [this]() {
        if (m_classicGoButton)
            m_classicGoButton->move(m_addressBar->width() - 22, 2);
    });
}

void MainWindow::applyChromeStyle()
{
    const Theme::Metrics metrics = Theme::metrics();
    const bool classic = Theme::current() == Theme::ThemeStyle::Classic;

    if (m_navToolbar)
        m_navToolbar->setIconSize(classic ? QSize(16, 16) : QSize(20, 20));
    if (m_menuBar)
        m_menuBar->setFixedHeight(metrics.menuBarHeight);

    // Classic Chrome has a permanent 20 px status bar: it is where link
    // targets, loading state and the "Done" message appeared. The classic
    // window relies on the OS border for resizing; the frameless modern window
    // uses a slimmer strip that hosts the size grip instead.
    if (statusBar()) {
        statusBar()->setFixedHeight(classic ? 20 : 16);
        statusBar()->setSizeGripEnabled(!classic);
    }

    if (m_tabs) {
        // The classic chrome's tab strip is its only tab surface (fixed 24 px
        // row); the modern one keeps Qt's natural height and lives beside the
        // vertical sidebar.
        m_tabs->tabBar()->setIconSize(QSize(16, 16));
        m_tabs->tabBar()->setExpanding(false);
        m_tabs->setDocumentMode(true);
        if (classic) {
            m_tabs->setHorizontalTabBarVisible(true);
            m_tabs->tabBar()->setFixedHeight(metrics.tabHeight + 4);
        } else {
            m_tabs->tabBar()->setMinimumHeight(0);
        }
    }

    if (m_bookmarkToolbar)
        m_bookmarkToolbar->setVisible(QSettings().value("browser/bookmarkBarVisible", true).toBool());
    if (m_homeAction)
        m_homeAction->setVisible(QSettings().value("browser/showHomeButton", true).toBool());

    // The settings page writes these two flags straight to QSettings, so the
    // live window has to re-read them after a settingsChanged() signal.
    if (m_settingsBridge) {
        connect(m_settingsBridge, &SettingsBridge::settingsChanged, this, [this]() {
            QSettings settings;
            if (m_bookmarkToolbar)
                m_bookmarkToolbar->setVisible(settings.value("browser/bookmarkBarVisible", true).toBool());
            if (m_homeAction)
                m_homeAction->setVisible(settings.value("browser/showHomeButton", true).toBool());
        }, Qt::UniqueConnection);
    }
}

QWebEngineView *MainWindow::activeView() const
{
    // While the split view is central, the tab widget is detached and has no
    // meaningful "current" view on screen; Back, Reload and zoom have to follow
    // what the user is actually looking at.
    if (m_splitView && m_splitView->isVisible()) {
        if (SplitPane *pane = m_splitView->activePane())
            return pane->getWebView();
        return nullptr;
    }
    return m_tabs ? m_tabs->currentView() : nullptr;
}

void MainWindow::toggleSplitView(bool checked)
{
    // QMainWindow::setCentralWidget() *deletes* the widget it replaces
    // (qmainwindow.cpp: `centralWidget()->hide(); centralWidget()->deleteLater();`).
    // Calling it to swap the panes in and out therefore destroyed m_browserSplitter
    // — and with it the tab widget and every renderer — the first time split view
    // was enabled, so turning it off again called setCentralWidget() on a dangling
    // pointer and took the whole process down.
    //
    // takeCentralWidget() detaches the current widget and hands ownership back to
    // us, which is what makes the swap reversible.
    if (m_tabs)
        m_tabs->hidePreview();

    if (checked) {
        if (!m_splitView) {
            const QUrl currentUrl = m_tabs && m_tabs->currentView()
                                        ? m_tabs->currentView()->url()
                                        : QUrl("cloudar://newtab");
            // Parented to the window, not adopted by setCentralWidget(): the
            // widget has to outlive being swapped out.
            m_splitView = new SplitView(m_profile, currentUrl, QUrl("cloudar://newtab"), this);
        }
        if (QWidget *previous = takeCentralWidget())
            previous->hide();
        m_splitView->show();
        setCentralWidget(m_splitView);
    } else {
        // Whatever is central right now is the split view; detach it so the
        // swap back to the tab widget is a hand-off rather than a deletion.
        if (QWidget *previous = takeCentralWidget())
            previous->hide();
        if (m_browserSplitter) {
            m_browserSplitter->show();
            setCentralWidget(m_browserSplitter);
        }
    }
    // The toolbar button and the View menu item are two separate QActions, so
    // whichever one was not used has to be brought in line — without re-entering
    // this slot, which is why the signal is blocked.
    for (QAction *action : {m_splitViewAction, m_splitViewMenuAction}) {
        if (!action || action->isChecked() == checked)
            continue;
        QSignalBlocker blocker(action);
        action->setChecked(checked);
    }
}
