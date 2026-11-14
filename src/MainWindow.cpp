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

#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QSizeGrip>
#include <QAction>
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

MainWindow::MainWindow(bool isIncognito, QWidget *parent)
    : QMainWindow(parent), m_isIncognito(isIncognito)
{
    setWindowIcon(QIcon(":/icon.png"));
    // Frameless: the window draws its own title bar (see buildTitleBar())
    // so its color can stay in sync with ThemeManager's accent color —
    // the native decoration a window manager draws can't be recolored
    // portably from Qt.
    setWindowFlag(Qt::FramelessWindowHint, true);
    if (m_isIncognito)
        setAttribute(Qt::WA_DeleteOnClose, true);
    // A frameless window has no window-manager resize border. Listening at
    // the application level keeps the edges usable over web content and
    // toolbars as well as the otherwise empty window frame.
    qApp->installEventFilter(this);

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
    m_profile->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    m_profile->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    m_profile->settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);

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

    connect(m_tabs, &BrowserTabWidget::urlChangedForCurrentTab, this, &MainWindow::updateAddressBar);
    connect(m_tabs, &BrowserTabWidget::urlChangedForCurrentTab, this, &MainWindow::updateBookmarkStar);
    connect(m_tabs, &BrowserTabWidget::titleChangedForCurrentTab, this, &MainWindow::updateWindowTitle);
    connect(m_tabs, &BrowserTabWidget::loadProgressForCurrentTab, this, &MainWindow::updateProgress);
    connect(m_tabs, &BrowserTabWidget::loadStartedForCurrentTab, this, [this]() {
        m_spinner->start();
        m_addressGlow->setEnabled(true);
        m_addressGlowAnim->start();
        updateNavigationState();
    });
    connect(m_tabs, &BrowserTabWidget::loadFinishedForCurrentTab, this, [this](bool) {
        m_spinner->stop();
        m_addressGlowAnim->stop();
        m_addressGlow->setBlurRadius(0);
        m_addressGlow->setEnabled(false);
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
    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::accentChanged, this,
            [this](const QColor &) { applyTheme(); });

    // Size grip in a slim status bar — the only way left to resize once
    // the window is frameless and the OS border is gone.
    auto *grip = new QSizeGrip(this);
    statusBar()->addPermanentWidget(grip);
    statusBar()->setFixedHeight(16);
    statusBar()->setSizeGripEnabled(false);

    // AISidebar dock widget
    m_aiSidebar = new AISidebar(this, this);
    m_aiSidebar->hide();
    addDockWidget(Qt::RightDockWidgetArea, m_aiSidebar);

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
    setSidebarVisible(settings.value("browser/sidebarEnabled", true).toBool());
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
}

void MainWindow::buildToolbar()
{
    auto *toolbar = addToolBar("Navigation");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20, 20));

    m_backAction = toolbar->addAction("\u2190"); // back
    m_backAction->setToolTip("Back");
    connect(m_backAction, &QAction::triggered, this, [this]() {
        if (auto *v = m_tabs->currentView()) v->back();
    });

    m_forwardAction = toolbar->addAction("\u2192"); // forward
    m_forwardAction->setToolTip("Forward");
    connect(m_forwardAction, &QAction::triggered, this, [this]() {
        if (auto *v = m_tabs->currentView()) v->forward();
    });

    m_reloadStopAction = toolbar->addAction("\u27F3"); // reload / stop while loading
    m_reloadStopAction->setToolTip("Reload");
    connect(m_reloadStopAction, &QAction::triggered, this, [this]() {
        if (auto *v = m_tabs->currentView()) {
            if (v->page()->isLoading()) v->stop(); else v->reload();
        }
    });

    m_addressBar = new QLineEdit(toolbar);
    m_addressBar->setPlaceholderText("Search or enter address");
    connect(m_addressBar, &QLineEdit::returnPressed, this, &MainWindow::navigateToAddressBarInput);
    toolbar->addWidget(m_addressBar);

    // A restrained "breathing" accent around the address bar makes page
    // loading visible even when the progress bar is near 0% or hidden.
    // It is disabled outside loads, so normal typing has no graphics-effect
    // compositing cost.
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

    m_bookmarkStarAction = toolbar->addAction("\u2606"); // outline star; filled (\u2605) when current page is bookmarked
    m_bookmarkStarAction->setToolTip("Bookmark this page");
    connect(m_bookmarkStarAction, &QAction::triggered, this, &MainWindow::toggleBookmarkForCurrentPage);

    auto *readerAction = toolbar->addAction("\u2637"); // reader/article glyph
    readerAction->setToolTip("Reader mode");
    connect(readerAction, &QAction::triggered, this, [this]() {
        auto *v = m_tabs->currentView();
        if (!v)
            return;
        v->page()->runJavaScript(ReaderMode::toggleScript(), [this](const QVariant &result) {
            if (result.toString() == "no-article")
                statusBar()->showMessage("Reader mode isn't available for this page", 3000);
        });
    });

    m_spinner = new SpinnerWidget(toolbar);
    toolbar->addWidget(m_spinner);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(120);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);
    toolbar->addWidget(m_progressBar);

    m_progressAnim = new QPropertyAnimation(m_progressBar, "value", this);
    m_progressAnim->setDuration(180);
    m_progressAnim->setEasingCurve(QEasingCurve::OutCubic);

    // AI Sidebar toggle button (Ctrl+Shift+A)
    m_aiSidebarAction = toolbar->addAction("AI"); // AI label for sidebar toggle
    m_aiSidebarAction->setToolTip("AI Sidebar (Ctrl+Shift+A)");
    m_aiSidebarAction->setCheckable(true);
    m_aiSidebarAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
    connect(m_aiSidebarAction, &QAction::triggered, this, [this](bool checked) {
        if (m_aiSidebar) {
            m_aiSidebar->setVisible(checked);
        }
    });

    // SplitView toggle button (⊞)
    m_splitViewAction = toolbar->addAction("\u29C9"); // ⧉ split view icon
    m_splitViewAction->setToolTip("Toggle Split View");
    m_splitViewAction->setCheckable(true);
    connect(m_splitViewAction, &QAction::triggered, this, [this](bool checked) {
        if (checked) {
            if (!m_splitView) {
                // Create split view the first time
                QUrl currentUrl = m_tabs->currentView() ? m_tabs->currentView()->url() : QUrl("cloudar://newtab");
                m_splitView = new SplitView(m_profile, currentUrl, QUrl("cloudar://newtab"));
            }
            m_splitView->show();
            setCentralWidget(m_splitView);
        } else {
            if (m_splitView)
                m_splitView->hide();
            setCentralWidget(m_browserSplitter);
        }
    });

    updateNavigationState();
}

void MainWindow::buildBookmarksBar()
{
    // addToolBarBreak forces the next toolbar onto a new row instead of
    // letting QMainWindow pack it into the same row as the nav toolbar —
    // the same trick buildTitleBar()'s comment explains for the title bar.
    addToolBarBreak(Qt::TopToolBarArea);

    auto *toolbar = addToolBar("Bookmarks");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->toggleViewAction()->setVisible(false); // don't let it show up in a right-click "toggle toolbar" menu
    toolbar->setContentsMargins(0, 0, 0, 0);

    m_bookmarkBar = new BookmarkBar(m_bookmarkManager, toolbar);
    connect(m_bookmarkBar, &BookmarkBar::bookmarkActivated, this, [this](const QUrl &url) {
        if (auto *v = m_tabs->currentView())
            v->load(url);
    });
    toolbar->addWidget(m_bookmarkBar);
}

void MainWindow::toggleBookmarkForCurrentPage()
{
    auto *v = m_tabs->currentView();
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
    m_bookmarkStarAction->setText(bookmarked ? "\u2605" : "\u2606");
    m_bookmarkStarAction->setToolTip(bookmarked ? "Remove bookmark" : "Bookmark this page");
}

void MainWindow::buildMenu()
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
    m_sidebarAction->setChecked(m_sidebar->isVisible());
    connect(m_sidebarAction, &QAction::toggled, this, &MainWindow::setSidebarVisible);

    auto *downloads = menu->addAction("Downloads");
    connect(downloads, &QAction::triggered, this, [this]() { m_downloadManager->showDialog(); });

    auto *history = menu->addAction("History");
    connect(history, &QAction::triggered, this, &MainWindow::showHistoryDialog);

    auto *find = menu->addAction("Find in page");
    find->setShortcut(QKeySequence::Find);
    connect(find, &QAction::triggered, this, &MainWindow::showFindBar);

    auto *zoom = menu->addMenu("Zoom");
    m_zoomInAction = zoom->addAction("Zoom In");
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this, [this](){ if (auto *v = m_tabs->currentView()) setZoom(v->zoomFactor() + .1); });
    m_zoomOutAction = zoom->addAction("Zoom Out");
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, [this](){ if (auto *v = m_tabs->currentView()) setZoom(v->zoomFactor() - .1); });
    m_zoomResetAction = zoom->addAction("Actual Size");
    m_zoomResetAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(m_zoomResetAction, &QAction::triggered, this, [this](){ setZoom(1.0); });

    auto *extensions = menu->addAction("Extensions");
    connect(extensions, &QAction::triggered, this, &MainWindow::showExtensionsDialog);

    menu->addSeparator();

    auto *settings = menu->addAction("Settings");
    connect(settings, &QAction::triggered, this, &MainWindow::openSettingsPage);

    auto *quit = menu->addAction("Quit");
    connect(quit, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::setSidebarVisible(bool visible)
{
    m_sidebar->setVisible(visible);
    m_browserSplitter->handle(1)->setVisible(visible);
    m_tabs->setHorizontalTabBarVisible(!visible);
    if (m_sidebarAction && m_sidebarAction->isChecked() != visible) {
        m_sidebarAction->blockSignals(true);
        m_sidebarAction->setChecked(visible);
        m_sidebarAction->blockSignals(false);
    }
    QSettings().setValue("browser/sidebarEnabled", visible);
}

void MainWindow::setSidebarExpanded(bool expanded, bool animated)
{
    if (!m_sidebar || !m_browserSplitter)
        return;
    const int target = expanded ? QSettings().value("browser/sidebarWidth", 240).toInt() : 54;
    QSettings().setValue("browser/sidebarExpanded", expanded);

    QList<int> sizes = m_browserSplitter->sizes();
    int currentWidth = sizes.isEmpty() ? 54 : sizes[0];

    if (!animated || !isVisible() || currentWidth == target) {
        m_browserSplitter->setSizes({target, qMax(1, m_browserSplitter->width() - target)});
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
        m_reloadStopAction->setText(loading ? "\u2715" : "\u27F3");
        m_reloadStopAction->setToolTip(loading ? "Stop" : "Reload");
    }
}

void MainWindow::showFindBar()
{
    auto *view = m_tabs->currentView();
    if (!view) return;
    bool accepted = false;
    const QString text = QInputDialog::getText(this, "Find in page", "Find:", QLineEdit::Normal, {}, &accepted);
    if (accepted && !text.isEmpty())
        view->findText(text);
}

void MainWindow::setZoom(qreal zoom)
{
    if (auto *view = m_tabs->currentView()) {
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
    QSettings settings;
    const QUrl origin = permission.origin();
    const QString key = QString("permissions/%1/%2").arg(origin.host(), QString::number(int(permission.permissionType())));
    const QString saved = settings.value(key).toString();
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
        settings.setValue(key, granted ? "allow" : "deny");
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
    QSettings settings;
    const QString key = QString("permissions/%1/%2").arg(origin.host(), QString::number(int(feature)));
    const QString saved = settings.value(key).toString();
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
        settings.setValue(key, granted ? "allow" : "deny");
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
    else if (auto *v = m_tabs->currentView())
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

    if (auto *v = m_tabs->currentView()) {
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
    m_titleBar->setTitle(full);
    if (!m_isIncognito && m_historyManager) {
        if (auto *v = m_tabs->currentView())
            m_historyManager->addVisit(title, v->url().toString());
    }
}

void MainWindow::updateProgress(int percent)
{
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
