#include "BrowserTabWidget.h"
#include "FaviconStore.h"
#include "TabPreviewPopup.h"
#include "WebExtensionManager.h"
#include "ClassicTheme.h"
#include "Language.h"
#include "ThemeManager.h"

#include <QWebEnginePage>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineFullScreenRequest>
#include <QToolButton>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QMouseEvent>
#include <QTabBar>
#include <QPainter>
#include <QPen>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QClipboard>
#include <QGuiApplication>
#include <cmath>

namespace {
constexpr int kThumbnailWidth = 240;
constexpr int kThumbnailHeight = 150;
constexpr int kHoverDelayMs = 450;
constexpr int kThumbnailRefreshMs = 2000;
}

BrowserTabWidget::BrowserTabWidget(QWebEngineProfile *profile, QWebChannel *channel, QWidget *parent)
    : QTabWidget(parent), m_profile(profile), m_channel(channel)
{
    setTabsClosable(true);
    // QTabBar draws the default close button as the style's own tiny cross,
    // which on a dark sheet is a grey smudge and never follows the light/dark
    // palette. tabInserted() installs the painted glyph instead (see
    // Theme::icon and the "close" case in ClassicTheme.cpp), and this keeps it
    // correct when the user flips light/dark without restarting.
    connect(&ThemeManager::instance(), &ThemeManager::colorModeChanged, this,
            [this](bool) { applyTabCloseGlyphs(); });
    setMovable(true);
    setDocumentMode(true);
    tabBar()->setExpanding(false);
    tabBar()->setElideMode(Qt::ElideRight);
    tabBar()->setIconSize(QSize(16, 16));

    connect(this, &QTabWidget::tabCloseRequested, this, &BrowserTabWidget::closeTabAt);

    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar(), &QTabBar::customContextMenuRequested, this, [this](const QPoint &pos) {
        int idx = tabBar()->tabAt(pos);
        if (idx >= 0) {
            dismissPreview();
            showTabContextMenu(idx, tabBar()->mapToGlobal(pos));
        }
    });

    // "+" button that lives right after the last tab in the tab strip
    // itself (like Chrome/Firefox), rather than off in the toolbar or
    // pinned to the far corner of the window.
    m_newTabButton = new QToolButton(tabBar());
    m_newTabButton->setObjectName("NewTabStripButton");
    m_newTabButton->setText("+");
    m_newTabButton->setAutoRaise(true);
    m_newTabButton->setCursor(Qt::PointingHandCursor);
    m_newTabButton->setFocusPolicy(Qt::NoFocus);
    m_newTabButton->setFixedSize(24, 24);
    connect(m_newTabButton, &QToolButton::clicked, this, [this]() { addNewTab(); });
    connect(tabBar(), &QTabBar::tabMoved, this, [this](int, int) {
        repositionNewTabButton();
        QMetaObject::invokeMethod(this, &BrowserTabWidget::repositionNewTabButton, Qt::QueuedConnection);
    });

    connect(this, &QTabWidget::currentChanged, this, [this](int index) {
        // Grab one last snapshot of the tab we're leaving while it's still
        // visible/rendered — this is what makes its thumbnail available
        // later even after it becomes a background tab.
        if (m_lastCurrentView)
            captureThumbnail(m_lastCurrentView);

        if (index < 0) {
            m_lastCurrentView = nullptr;
            return;
        }
        // A slept tab's placeholder is not a web view: waking it rebuilds the
        // real page in place, and the resulting currentChanged() call lands
        // back here with a proper QWebEngineView.
        if (m_sleptUrls.contains(index)) {
            const QUrl sleptUrl(m_sleptUrls.value(index));
            QMetaObject::invokeMethod(this, [this, index]() { reopenTabAt(index); },
                                      Qt::QueuedConnection);
            emit urlChangedForCurrentTab(sleptUrl);
            return;
        }

        auto *view = qobject_cast<QWebEngineView *>(widget(index));
        if (!view)
            return;
        m_lastCurrentView = view;
        emit urlChangedForCurrentTab(view->url());
        emit titleChangedForCurrentTab(view->title());
        emit currentTabChanged(index);
    });

    // Periodically refresh the *current* tab's thumbnail so a long-lived
    // page (video, dashboard, etc.) doesn't show a stale first-load
    // snapshot forever.
    m_thumbnailTimer = new QTimer(this);
    m_thumbnailTimer->setInterval(kThumbnailRefreshMs);
    connect(m_thumbnailTimer, &QTimer::timeout, this, [this]() {
        if (auto *v = currentView())
            captureThumbnail(v);
    });
    m_thumbnailTimer->start();

    m_previewPopup = new TabPreviewPopup(this); // top-level window (Qt::ToolTip flag) but parented for lifetime/stacking
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    connect(m_previewTimer, &QTimer::timeout, this, [this]() { showPreviewForTab(m_hoveredTabIndex); });

    tabBar()->setMouseTracking(true);
    tabBar()->installEventFilter(this);

    repositionNewTabButton();
}

bool BrowserTabWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == tabBar()) {
        if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            int idx = tabBar()->tabAt(mouseEvent->pos());
            if (idx != m_hoveredTabIndex) {
                m_hoveredTabIndex = idx;
                dismissPreview();
                m_previewTimer->stop();
                if (idx >= 0)
                    m_previewTimer->start(kHoverDelayMs);
            }
        } else if (event->type() == QEvent::Leave) {
            m_hoveredTabIndex = -1;
            m_previewTimer->stop();
            dismissPreview();
        } else if (event->type() == QEvent::Resize ||
                   event->type() == QEvent::LayoutRequest ||
                   event->type() == QEvent::Show ||
                   event->type() == QEvent::Move) {
            repositionNewTabButton();
        }
    }
    return QTabWidget::eventFilter(watched, event);
}

void BrowserTabWidget::tabInserted(int index)
{
    QTabWidget::tabInserted(index);
    // Queued: QTabWidget has not created the style's close button yet at this
    // point, and applyCloseGlyph() needs to replace whatever is there.
    QMetaObject::invokeMethod(this, [this, index]() { applyCloseGlyph(index); },
                              Qt::QueuedConnection);
    repositionNewTabButton();
    QMetaObject::invokeMethod(this, &BrowserTabWidget::repositionNewTabButton, Qt::QueuedConnection);
}

void BrowserTabWidget::tabRemoved(int index)
{
    QTabWidget::tabRemoved(index);
    repositionNewTabButton();
    QMetaObject::invokeMethod(this, &BrowserTabWidget::repositionNewTabButton, Qt::QueuedConnection);
}

void BrowserTabWidget::resizeEvent(QResizeEvent *event)
{
    QTabWidget::resizeEvent(event);
    repositionNewTabButton();
}

void BrowserTabWidget::repositionNewTabButton()
{
    if (!m_newTabButton || !tabBar() || !tabBar()->isVisible())
        return;

    // The + sits inline right after the last tab, vertically centered in the
    // strip (Firefox/Chrome-style).
    constexpr int spacing = 4;
    constexpr int targetBtnSize = 24;
    const int barH = tabBar()->height();
    const int barW = tabBar()->width();

    const int btnH = (barH > 0) ? qMin(targetBtnSize, qMax(16, barH - 4)) : targetBtnSize;
    const int btnW = targetBtnSize;
    m_newTabButton->setFixedSize(btnW, btnH);

    int x = spacing;
    if (tabBar()->count() > 0) {
        const QRect lastTabRect = tabBar()->tabRect(tabBar()->count() - 1);
        x = lastTabRect.right() + 1 + spacing;
    }
    if (barW > 0 && (x + btnW + spacing > barW)) {
        x = barW - btnW - spacing;
    }

    const int y = (barH > 0) ? (barH - btnH) / 2 : 0;

    m_newTabButton->move(qMax(spacing, x), qMax(0, y));
    m_newTabButton->raise(); // keep it above the tab bar / overlapping page
    m_newTabButton->show();
}

void BrowserTabWidget::captureThumbnail(QWebEngineView *view)
{
    if (!view || !view->isVisible())
        return;
    QPixmap pix = view->grab();
    if (pix.isNull())
        return;
    m_thumbnails[view] = pix.scaled(kThumbnailWidth, kThumbnailHeight,
                                     Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
}

void BrowserTabWidget::showPreviewForTab(int index)
{
    if (index < 0 || index >= count())
        return;

    // A slept tab still deserves a hover card — it just has no live view to
    // read a URL from, so fall back to the URL we remembered.
    if (m_sleptUrls.contains(index)) {
        m_previewPopup->setContent(QPixmap(), tabText(index),
                                    QUrl(m_sleptUrls.value(index)),
                                    QStringLiteral("sleeping"));
        const QRect sleptRect = tabBar()->tabRect(index);
        m_previewPopup->move(tabBar()->mapToGlobal(QPoint(sleptRect.left(), sleptRect.bottom() + 6)));
        m_previewPopup->fadeIn();
        return;
    }

    auto *view = qobject_cast<QWebEngineView *>(widget(index));
    if (!view)
        return;

    const bool active = (index == currentIndex());
    m_previewPopup->setContent(tabThumbnail(index), tabText(index), view->url(),
                                active ? QStringLiteral("active")
                                       : QStringLiteral("background"));

    const QRect tabRect = tabBar()->tabRect(index);
    const QPoint anchor = tabBar()->mapToGlobal(QPoint(tabRect.left(), tabRect.bottom() + 6));
    m_previewPopup->move(anchor);
    m_previewPopup->fadeIn();
}

void BrowserTabWidget::dismissPreview()
{
    m_previewPopup->fadeOut();
}

void BrowserTabWidget::wireView(QWebEngineView *view)
{
    // Right-click menu. QtWebEngine's own default is a bare list (see
    // PageContextMenu) and on some platforms it is absent entirely, so every tab,
    // including the ones restored from a session, gets ours.
    if (m_contextMenuFactory)
        m_contextMenuFactory(view);

    connect(view, &QWebEngineView::iconChanged, this, [this, view](const QIcon &icon) {
        // Feed the favicon store first: it keys on the page URL, and the tab
        // strip's own icon handling below may replace a null icon with a
        // generated placeholder that would be wrong to cache as the site's icon.
        if (m_favicons && !icon.isNull())
            m_favicons->record(view->url(), icon);

        int idx = indexOf(view);
        if (idx >= 0) {
            const QIcon eff = icon.isNull() ? defaultFavicon(view->url()) : icon;
            setTabIcon(idx, eff);
            emit tabIconChanged(idx, eff);
            emit tabsChanged();
        }
    });

    connect(view, &QWebEngineView::titleChanged, this, [this, view](const QString &title) {
        int idx = indexOf(view);
        if (idx >= 0) {
            setTabText(idx, title.isEmpty() ? get_text("new_tab_title") : title);
            if (tabIcon(idx).isNull())
                setTabIcon(idx, defaultFavicon(view->url()));
            repositionNewTabButton();
            QMetaObject::invokeMethod(this, &BrowserTabWidget::repositionNewTabButton, Qt::QueuedConnection);
        }
        emit tabsChanged();
        if (currentWidget() == view)
            emit titleChangedForCurrentTab(title);
    });

    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &url) {
        int idx = indexOf(view);
        if (idx >= 0 && view->icon().isNull()) {
            const QIcon eff = defaultFavicon(url);
            setTabIcon(idx, eff);
            emit tabIconChanged(idx, eff);
        }

        // Only expose the native bridge to our own internal pages — a
        // regular website should never be able to call into NativeBridge.
        // Ordinary pages instead get the (isolated-world) extensions
        // channel, so enabled real extensions' content scripts can reach
        // chrome.* without that bridge ever being reachable from page JS.
        if (url.scheme() == "light" || url.scheme() == "browser" || url.scheme() == "cloudar") {
            if (m_channel)
                view->page()->setWebChannel(m_channel);
        } else if (m_webExtensions) {
            m_webExtensions->attachToPage(view->page());
        } else if (m_channel) {
            view->page()->setWebChannel(nullptr);
        }

        if (currentWidget() == view)
            emit urlChangedForCurrentTab(url);
        emit tabsChanged();
    });

    connect(view, &QWebEngineView::loadProgress, this, [this, view](int percent) {
        if (currentWidget() == view)
            emit loadProgressForCurrentTab(percent);
    });

    connect(view, &QWebEngineView::loadStarted, this, [this, view]() {
        if (currentWidget() == view)
            emit loadStartedForCurrentTab();
    });

    connect(view, &QWebEngineView::loadFinished, this, [this, view](bool ok) {
        // Every internal page reads this key to decide whether to render its
        // classic (2009) or modern variant, and localStorage is the only
        // channel those pages share. Writing it on each internal load means a
        // theme switch is picked up by the next cloudar:// page even before the
        // window itself is restarted.
        const QUrl loadedUrl = view->url();
        if (ok && (loadedUrl.scheme() == QLatin1String("cloudar")
                   || loadedUrl.scheme() == QLatin1String("browser")
                   || loadedUrl.scheme() == QLatin1String("light"))) {
            const QString styleKey = Theme::keyFor(Theme::current());
            view->page()->runJavaScript(
                QStringLiteral("try{localStorage.setItem('cloudar.chromeStyle','%1')}catch(e){}")
                    .arg(styleKey));

            // The same channel carries the light/dark decision. The *resolved*
            // answer is written, not the preference: a page cannot query
            // QStyleHints, and "system" would leave it guessing. The classic
            // chrome keeps "auto", and its CSS ignores the attribute anyway —
            // its palette is fixed by design.
            const QString colorMode =
                    Theme::current() == Theme::ThemeStyle::Modern
                        ? (ThemeManager::instance().prefersDark() ? QStringLiteral("dark")
                                                                  : QStringLiteral("light"))
                        : QStringLiteral("auto");
            view->page()->runJavaScript(
                QStringLiteral("try{"
                               "localStorage.setItem('cloudar.colorMode','%1');"
                               "document.documentElement.setAttribute('data-color-mode','%1');"
                               "}catch(e){}")
                    .arg(colorMode));
        }

        if (currentWidget() == view) {
            emit loadFinishedForCurrentTab(ok);
            captureThumbnail(view);
        }
    });

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(view->page(), &QWebEnginePage::permissionRequested, this,
            [this](const QWebEnginePermission &permission) {
                emit permissionRequested(permission);
            });
#else
    connect(view->page(), &QWebEnginePage::featurePermissionRequested, this,
            [this, page = view->page()](const QUrl &origin, QWebEnginePage::Feature feature) {
                emit featurePermissionRequested(origin, feature, page);
            });
#endif

    // Fullscreen video request support.
    connect(view->page(), &QWebEnginePage::fullScreenRequested, this,
            [](QWebEngineFullScreenRequest request) {
                request.accept();
            });

    // Middle-click / ctrl+click "open in new tab" support.
    connect(view->page(), &QWebEnginePage::newWindowRequested, this,
            [this](QWebEngineNewWindowRequest &request) {
                auto *view = addNewTab(request.requestedUrl(), true);
                request.openIn(view->page());
            });
}

void BrowserTabWidget::animateIn(QWidget *view)
{
    auto *effect = new QGraphicsOpacityEffect(view);
    view->setGraphicsEffect(effect);

    auto *anim = new QPropertyAnimation(effect, "opacity", view);
    anim->setDuration(220);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    // Drop the effect afterwards — QGraphicsOpacityEffect forces the widget
    // through the software compositing path, which we don't want to pay
    // for on every repaint once the fade-in is done.
    connect(anim, &QPropertyAnimation::finished, view, [view]() {
        view->setGraphicsEffect(nullptr);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

QWebEngineView *BrowserTabWidget::addNewTab(const QUrl &url, bool makeCurrent)
{
    auto *view = new QWebEngineView(this);
    auto *page = new QWebEnginePage(m_profile, view);
    view->setPage(page);
    wireView(view);

    int idx = addTab(view, defaultFavicon(url), get_text("new_tab_title"));
    if (makeCurrent)
        setCurrentIndex(idx);

    view->load(url);
    animateIn(view);
    repositionNewTabButton();
    emit tabsChanged();
    return view;
}

void BrowserTabWidget::applyCloseGlyph(int index)
{
    if (index < 0 || index >= count())
        return;
    QAbstractButton *button = tabBar()->tabButton(index, QTabBar::RightSide);
    auto *tool = qobject_cast<QToolButton *>(button);
    // Two cases reach the `!tool` branch: Qt created the button but as a plain
    // QTabBar::CloseButton (its own painted cross), or it created nothing. The
    // first must be deleted before it is replaced, or QTabBar keeps a child
    // button that is no longer the tab's and paints a second cross over ours.
    if (button && !tool) {
        // setTabButton() only accepts a QWidget*, and Qt's own close button is
        // a QTabBar::CloseButton (a QAbstractButton) — so the clear is a plain
        // QWidget* cast of nullptr, which is where the compiler needed help.
        tabBar()->setTabButton(index, QTabBar::RightSide, static_cast<QWidget *>(nullptr));
        delete button;
    }
    if (!tool) {
        // The style has not created the button yet (it does so on the first
        // paint); create our own so the glyph is right from the first frame.
        tool = new QToolButton(tabBar());
        tool->setObjectName(QStringLiteral("TabCloseButton"));
        tool->setAutoRaise(true);
        tool->setFocusPolicy(Qt::NoFocus);
        tool->setIconSize(QSize(12, 12));
        tool->setCursor(Qt::ArrowCursor);
        tabBar()->setTabButton(index, QTabBar::RightSide, tool);
        connect(tool, &QToolButton::clicked, this, [this, tool]() {
            // Resolved through the button's own position: tabs move, so a
            // captured index would close the wrong one after a drag.
            const int fromButton = tabBar()->tabAt(
                tabBar()->mapFromGlobal(tool->mapToGlobal(tool->rect().center())));
            if (fromButton >= 0)
                closeTabAt(fromButton);
        });
        tool->setToolTip(get_text("tab_close"));
    }
    tool->setIcon(Theme::icon(QStringLiteral("close"), 12));
}

void BrowserTabWidget::applyTabCloseGlyphs()
{
    for (int i = 0; i < count(); ++i)
        applyCloseGlyph(i);
}

void BrowserTabWidget::closeTabAt(int index)
{
    QWidget *w = widget(index);
    if (!w)
        return;

    if (auto *view = qobject_cast<QWebEngineView *>(w)) {
        const QUrl u = view->url();
        if (!u.isEmpty() && u != QUrl("cloudar://newtab") && u != QUrl("browser://newtab")) {
            m_closedTabUrls.prepend(u.toString());
            if (m_closedTabUrls.size() > 25)
                m_closedTabUrls.removeLast();
        }
        m_thumbnails.remove(view);
        if (m_lastCurrentView == view)
            m_lastCurrentView = nullptr;
    }
    if (index == m_hoveredTabIndex) {
        m_hoveredTabIndex = -1;
        m_previewTimer->stop();
        dismissPreview();
    }

    // Sleeping tabs are tracked by index, so every index above the closed
    // one shifts down by one.
    if (!m_sleptUrls.isEmpty()) {
        QHash<int, QString> shifted;
        for (auto it = m_sleptUrls.constBegin(); it != m_sleptUrls.constEnd(); ++it) {
            if (it.key() == index)
                continue;
            shifted.insert(it.key() > index ? it.key() - 1 : it.key(), it.value());
        }
        m_sleptUrls = shifted;
    }

    removeTab(index);
    w->deleteLater();

    // Never leave the browser with zero tabs open.
    if (count() == 0)
        addNewTab();
    else
        repositionNewTabButton();
    emit tabsChanged();
}

bool BrowserTabWidget::snapshotTab(int index, TabSnapshot *snapshot) const
{
    if (!snapshot || index < 0 || index >= count())
        return false;

    snapshot->title = tabText(index);
    snapshot->icon = tabIcon(index);

    // A slept tab keeps its URL on the side (see m_sleptUrls) but has no page.
    if (m_sleptUrls.contains(index)) {
        snapshot->url = QUrl(m_sleptUrls.value(index));
        return !snapshot->url.isEmpty();
    }

    auto *view = qobject_cast<QWebEngineView *>(widget(index));
    if (!view)
        return false;

    snapshot->url = view->url();
    return !snapshot->url.isEmpty();
}

QWebEngineView *BrowserTabWidget::currentView() const
{
    return qobject_cast<QWebEngineView *>(currentWidget());
}

void BrowserTabWidget::setHorizontalTabBarVisible(bool visible)
{
    tabBar()->setVisible(visible);
    m_newTabButton->setVisible(visible);
    if (visible) {
        repositionNewTabButton();
        QMetaObject::invokeMethod(this, &BrowserTabWidget::repositionNewTabButton, Qt::QueuedConnection);
    }
}

QStringList BrowserTabWidget::openUrls() const
{
    QStringList urls;
    for (int i = 0; i < count(); ++i) {
        // A slept tab has no QWebEngineView, but its URL is remembered and is
        // still part of the session the user expects to get back.
        if (m_sleptUrls.contains(i)) {
            const QUrl slept(m_sleptUrls.value(i));
            if (slept.isValid() && !slept.isEmpty())
                urls.append(slept.toString());
            continue;
        }
        if (auto *view = qobject_cast<QWebEngineView *>(widget(i))) {
            const QUrl url = view->url();
            // An empty URL is a transient construction state, not a useful
            // session entry.
            if (url.isValid() && !url.isEmpty())
                urls.append(url.toString());
        }
    }
    return urls;
}

void BrowserTabWidget::restoreTabs(const QStringList &urls, int currentIndex)
{
    if (urls.isEmpty())
        return;
    for (const QString &value : urls) {
        const QUrl url(value);
        if (url.isValid() && !url.isEmpty())
            addNewTab(url, false);
    }
    if (count() > 0)
        setCurrentIndex(qBound(0, currentIndex, count() - 1));
}

bool BrowserTabWidget::focusOrOpenTab(const QString &urlPrefix, const QUrl &url)
{
    for (int i = 0; i < count(); ++i) {
        auto *view = qobject_cast<QWebEngineView *>(widget(i));
        if (view && view->url().toString().startsWith(urlPrefix, Qt::CaseInsensitive)) {
            setCurrentIndex(i);
            return true;
        }
    }
    addNewTab(url);
    return false;
}

int BrowserTabWidget::openTabForExtension(const QUrl &url, bool active)
{
    QWebEngineView *view = addNewTab(url, active);
    return view ? indexOf(view) : -1;
}

bool BrowserTabWidget::updateTabForExtension(int index, const QUrl &url)
{
    auto *view = qobject_cast<QWebEngineView *>(widget(index));
    if (!view)
        return false;
    view->setUrl(url);
    return true;
}

bool BrowserTabWidget::closeTabForExtension(int index)
{
    if (index < 0 || index >= count())
        return false;
    closeTabAt(index);
    return true;
}

QString BrowserTabWidget::urlForTabIndex(int index) const
{
    if (m_sleptUrls.contains(index))
        return m_sleptUrls.value(index);
    auto *view = qobject_cast<QWebEngineView *>(widget(index));
    return view ? view->url().toString() : QString();
}

void BrowserTabWidget::runJavaScriptInTab(int index, const QString &code,
                                           std::function<void(const QString &)> callback)
{
    auto *view = qobject_cast<QWebEngineView *>(widget(index));
    if (!view || !view->page()) {
        callback(QStringLiteral("{\"error\":\"no such tab\"}"));
        return;
    }
    // Wrapped as {"result": <value>} rather than serialized bare, since a
    // top-level JS primitive (string/number/bool/null) isn't valid at the
    // root of a QJsonDocument — the extension-facing shim unwraps this.
    view->page()->runJavaScript(code, [callback](const QVariant &result) {
        QJsonObject wrapper;
        wrapper.insert("result", QJsonValue::fromVariant(result));
        callback(QString::fromUtf8(QJsonDocument(wrapper).toJson(QJsonDocument::Compact)));
    });
}

QIcon BrowserTabWidget::tabIcon(int index) const
{
    if (index < 0 || index >= count())
        return QIcon();
    auto *view = qobject_cast<QWebEngineView *>(widget(index));
    if (view && !view->icon().isNull())
        return view->icon();
    if (view)
        return defaultFavicon(view->url());
    return QTabWidget::tabIcon(index);
}

QPixmap BrowserTabWidget::tabThumbnail(int index) const
{
    if (index < 0 || index >= count())
        return QPixmap();
    auto *view = qobject_cast<QWebEngineView *>(widget(index));
    if (!view)
        return QPixmap();
    if (m_thumbnails.contains(view))
        return m_thumbnails.value(view);
    if (view->isVisible()) {
        QPixmap pix = view->grab();
        if (!pix.isNull())
            return pix.scaled(kThumbnailWidth, kThumbnailHeight,
                              Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }
    return QPixmap();
}

QIcon BrowserTabWidget::defaultFavicon(const QUrl &url)
{
    const QString str = url.toString().toLower();
    if (str.contains("settings")) {
        QPixmap pix(32, 32);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(QColor("#9aa4b8"), 2.2);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.drawEllipse(QRectF(11, 11, 10, 10));
        for (int i = 0; i < 6; ++i) {
            qreal a = i * 3.14159265358979323846 / 3.0;
            qreal x1 = 16.0 + 8.0 * std::cos(a);
            qreal y1 = 16.0 + 8.0 * std::sin(a);
            qreal x2 = 16.0 + 13.0 * std::cos(a);
            qreal y2 = 16.0 + 13.0 * std::sin(a);
            p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
        }
        p.end();
        return QIcon(pix);
    }

    if (str.contains("newtab") || str.contains("about") || str.contains("version") || str.isEmpty() || str == "about:blank") {
        if (QFile::exists(":/icon.png"))
            return QIcon(":/icon.png");
    }

    // Default Web Globe Icon (Chrome style)
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#9aa4b8"), 2.0);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawEllipse(QRectF(4, 4, 24, 24));
    p.drawLine(QPointF(4, 16), QPointF(28, 16));
    p.drawEllipse(QRectF(10, 4, 12, 24));
    p.end();
    return QIcon(pix);
}

void BrowserTabWidget::reopenClosedTab()
{
    if (!m_closedTabUrls.isEmpty()) {
        const QString urlStr = m_closedTabUrls.takeFirst();
        addNewTab(QUrl(urlStr), true);
    }
}

// ---------------------------------------------------------------------------
// Memory Saver
//
// QtWebEngine gives us no public "discard this tab's renderer" call, so the
// only way to actually return a background tab's memory to the OS is to drop
// its QWebEngineView (and with it the QWebEnginePage / renderer process) and
// remember the URL. The replacement widget is a plain QLabel: no engine, no
// process, a few kilobytes.
// ---------------------------------------------------------------------------

bool BrowserTabWidget::isTabSlept(int index) const
{
    return m_sleptUrls.contains(index);
}

bool BrowserTabWidget::sleepTab(int index)
{
    if (index < 0 || index >= count() || index == currentIndex())
        return false;
    if (m_sleptUrls.contains(index))
        return false; // already asleep

    auto *view = qobject_cast<QWebEngineView *>(widget(index));
    if (!view)
        return false;

    const QUrl url = view->url();
    if (!url.isValid() || url.isEmpty() || url.scheme() == QLatin1String("about"))
        return false;

    const QString title = tabText(index);
    const QIcon icon = tabIcon(index);

    m_sleptUrls.insert(index, url.toString());
    m_thumbnails.remove(view);

    // A lightweight stand-in that keeps the tab visually present in both the
    // strip and the vertical sidebar, with a hint explaining what happened.
    auto *placeholder = new QWidget(this);
    auto *layout = new QVBoxLayout(placeholder);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignCenter);

    auto *headline = new QLabel(tr("This tab is sleeping to save memory"), placeholder);
    QFont headlineFont = headline->font();
    headlineFont.setPointSizeF(headlineFont.pointSizeF() + 2.0);
    headlineFont.setBold(true);
    headline->setFont(headlineFont);
    headline->setAlignment(Qt::AlignCenter);

    auto *detail = new QLabel(url.toString(), placeholder);
    detail->setAlignment(Qt::AlignCenter);
    detail->setWordWrap(true);
    detail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detail->setStyleSheet(QStringLiteral("color: palette(mid);"));

    auto *wake = new QPushButton(tr("Reload this tab"), placeholder);
    wake->setCursor(Qt::PointingHandCursor);
    connect(wake, &QPushButton::clicked, this, [this, placeholder]() {
        const int idx = indexOf(placeholder);
        if (idx >= 0)
            setCurrentIndex(idx);
    });

    layout->addWidget(headline);
    layout->addWidget(detail);
    layout->addWidget(wake, 0, Qt::AlignCenter);

    removeTab(index);
    view->deleteLater();
    insertTab(index, placeholder, icon, title);
    setTabToolTip(index, tr("%1 (sleeping)").arg(url.toString()));
    repositionNewTabButton();
    emit tabsChanged();
    return true;
}

bool BrowserTabWidget::reopenTabAt(int index)
{
    if (!m_sleptUrls.contains(index))
        return false;
    const QUrl url(m_sleptUrls.take(index));
    if (index < 0 || index >= count())
        return false;

    QWidget *placeholder = widget(index);
    const QString title = tabText(index);
    removeTab(index);
    if (placeholder)
        placeholder->deleteLater();

    auto *view = addNewTab(url, true);
    if (!view)
        return false;
    const int newIndex = indexOf(view);
    if (newIndex != index) {
        tabBar()->moveTab(newIndex, index);
        setCurrentIndex(index);
    }
    if (!title.isEmpty())
        setTabText(index, title);
    emit tabsChanged();
    return true;
}

void BrowserTabWidget::retranslate()
{
    for (int i = 0; i < count(); ++i) {
        // Only the placeholders this widget itself wrote are rewritten. A real
        // page title is the site's text, not ours to translate, and a tab whose
        // title is still the empty-state one has not loaded a page yet.
        const QString current = tabText(i);
        if (current == QLatin1String("New Tab") || current.isEmpty())
            setTabText(i, get_text("new_tab_title"));
    }
    repositionNewTabButton();
}

void BrowserTabWidget::showTabContextMenu(int index, const QPoint &globalPos)
{
    if (index < 0 || index >= count())
        return;

    auto *view = qobject_cast<QWebEngineView *>(widget(index));
    if (!view)
        return;

    QMenu menu(this);

    QAction *newTabRight = menu.addAction(tr("New tab to the right"));
    QAction *reload = menu.addAction(tr("Reload"));
    reload->setShortcut(QKeySequence::Refresh);
    QAction *dup = menu.addAction(tr("Duplicate tab"));

    menu.addSeparator();

    const bool isMuted = view->page() ? view->page()->isAudioMuted() : false;
    QAction *mute = menu.addAction(isMuted ? tr("Unmute tab") : tr("Mute tab"));
    QAction *copyUrl = menu.addAction(tr("Copy link address"));

    menu.addSeparator();

    QAction *close = menu.addAction(tr("Close tab"));
    close->setShortcut(QKeySequence::Close);

    QAction *closeOthers = menu.addAction(tr("Close other tabs"));
    closeOthers->setEnabled(count() > 1);

    QAction *closeRight = menu.addAction(tr("Close tabs to the right"));
    closeRight->setEnabled(index < count() - 1);

    QAction *closeLeft = menu.addAction(tr("Close tabs to the left"));
    closeLeft->setEnabled(index > 0);

    menu.addSeparator();

    QAction *reopen = menu.addAction(tr("Reopen closed tab"));
    reopen->setShortcut(QKeySequence("Ctrl+Shift+T"));
    reopen->setEnabled(!m_closedTabUrls.isEmpty());

    QAction *chosen = menu.exec(globalPos);
    if (!chosen)
        return;

    if (chosen == newTabRight) {
        auto *newView = addNewTab(QUrl("cloudar://newtab"), true);
        int newIdx = indexOf(newView);
        if (newIdx != -1 && newIdx != index + 1) {
            tabBar()->moveTab(newIdx, index + 1);
        }
    } else if (chosen == reload) {
        view->reload();
    } else if (chosen == dup) {
        auto *newView = addNewTab(view->url(), true);
        int newIdx = indexOf(newView);
        if (newIdx != -1 && newIdx != index + 1) {
            tabBar()->moveTab(newIdx, index + 1);
        }
    } else if (chosen == mute) {
        if (view->page())
            view->page()->setAudioMuted(!isMuted);
    } else if (chosen == copyUrl) {
        QGuiApplication::clipboard()->setText(view->url().toString());
    } else if (chosen == close) {
        closeTabAt(index);
    } else if (chosen == closeOthers) {
        for (int i = count() - 1; i >= 0; --i) {
            if (i != index)
                closeTabAt(i);
        }
    } else if (chosen == closeRight) {
        for (int i = count() - 1; i > index; --i) {
            closeTabAt(i);
        }
    } else if (chosen == closeLeft) {
        for (int i = index - 1; i >= 0; --i) {
            closeTabAt(i);
        }
    } else if (chosen == reopen) {
        reopenClosedTab();
    }
}

