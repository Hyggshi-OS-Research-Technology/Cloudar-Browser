#include "BrowserSidebar.h"
#include "BrowserTabWidget.h"
#include "ClassicTheme.h"
#include "ThemeManager.h"
#include "Language.h"

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QToolButton>
#include <QWebEngineView>
#include <QStyle>
#include <QTabBar>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QEnterEvent>
#include <QGraphicsDropShadowEffect>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>

namespace {
// Kept in sync with Theme::Metrics::sidebarCollapsedWidth / ExpandedWidth; the
// classic chrome hides this widget entirely, so these only describe the modern
// chrome.
constexpr int kCollapsedWidth = 54;
constexpr int kExpandedMaxWidth = 420;

// The drag payload. A private MIME type, not text/uri-list: nothing else in the
// application (and no website) should be able to accept a dragged tab, and a
// dragged URL from a page should not look like one of ours.
const char *const kTabMimeType = "application/x-cloudar-sidebar-tab";

// Where a dragged row would land, expressed as a gap between rows: 0 is "above
// the first row", count() is "below the last one". Gaps rather than item
// indices because dropping *between* two rows is the only thing a reorder
// means, and it removes the off-by-one that QTabBar::moveTab() semantics
// otherwise invite.
int gapForPosition(QListWidget *list, int y)
{
    for (int i = 0; i < list->count(); ++i) {
        const QRect r = list->visualItemRect(list->item(i));
        if (y < r.center().y())
            return i;
    }
    return list->count();
}

// Turns a gap in the *current* order into the destination index moveTab() wants.
// Once the dragged tab is lifted out, everything after its old position shifts
// down by one, so a gap below it lands one slot earlier.
int destinationForGap(int gap, int from)
{
    return gap > from ? gap - 1 : gap;
}


// Solid Floating Chrome-style Tab Hover Card with Real Thumbnail Preview
class SidebarHoverCard : public QFrame
{
public:
    explicit SidebarHoverCard(QWidget *parent = nullptr) : QFrame(parent)
    {
        setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TranslucentBackground);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(6);

        m_titleLabel = new QLabel(this);
        // Same rule as TabPreviewPopup: these cards float over the page, so their
        // colours come from the active chrome instead of the modern palette.
        const Theme::PopupColors colors = Theme::popupColors();
        m_titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 700; background: transparent;")
                                        .arg(colors.text.name()));
        m_titleLabel->setWordWrap(true);
        m_titleLabel->setMaximumWidth(260);
        layout->addWidget(m_titleLabel);

        m_domainLabel = new QLabel(this);
        m_domainLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 500; background: transparent;")
                                         .arg(colors.dim.name()));
        layout->addWidget(m_domainLabel);

        m_thumbnailLabel = new QLabel(this);
        m_thumbnailLabel->setFixedSize(260, 146);
        m_thumbnailLabel->setScaledContents(true);
        m_thumbnailLabel->setAlignment(Qt::AlignCenter);
        m_thumbnailLabel->setStyleSheet(
            QStringLiteral("background-color: %1; border-radius: 4px; border: 1px solid %2;")
                .arg(colors.placeholder.name(), colors.border.name())
        );
        layout->addWidget(m_thumbnailLabel);

        auto *divider = new QFrame(this);
        divider->setFrameShape(QFrame::HLine);
        divider->setStyleSheet(
        // A white 8% hairline is invisible on a light surface: it assumes the
        // dark card. The border colour works in both.
        QStringLiteral("background: %1; max-height: 1px; min-height: 1px; border: none;")
            .arg(colors.border.name()));
        layout->addWidget(divider);

        auto *footerLayout = new QHBoxLayout;
        footerLayout->setContentsMargins(0, 2, 0, 0);
        footerLayout->setSpacing(6);

        m_stateIcon = new QLabel(this);
        m_stateIcon->setStyleSheet(QStringLiteral("font-size: 12px; background: transparent;"));
        footerLayout->addWidget(m_stateIcon);

        m_stateLabel = new QLabel(this);
        m_stateLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 500; background: transparent;")
                                        .arg(colors.dim.name()));
        footerLayout->addWidget(m_stateLabel);
        footerLayout->addStretch();
        layout->addLayout(footerLayout);

        auto *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(24);
        shadow->setColor(QColor(0, 0, 0, 200));
        shadow->setOffset(0, 4);
        setGraphicsEffect(shadow);
    }

    void setTabInfo(const QString &title, const QUrl &url, const QPixmap &thumbnail,
                    const QString &state)
    {
        m_titleLabel->setText(title.isEmpty() ? get_text("new_tab_title") : title);

        QString domain;
        if (url.scheme() == "cloudar" || url.scheme() == "browser" || url.scheme() == "light") {
            domain = url.toString();
        } else if (!url.host().isEmpty()) {
            domain = url.host();
        } else {
            domain = url.toString();
        }
        m_domainLabel->setText(domain.isEmpty() ? QStringLiteral("cloudar://newtab/") : domain);

        if (!thumbnail.isNull()) {
            QPixmap rounded(260, 146);
            rounded.fill(Qt::transparent);
            QPainter p(&rounded);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            // The thumbnail's corners track the card's: the classic chrome uses a
            // 3 px radius everywhere, the modern one 8 px here and 12 px on the card.
            const qreal cornerRadius = Theme::current() == Theme::ThemeStyle::Classic ? 3.0 : 8.0;
            path.addRoundedRect(QRectF(0, 0, 260, 146), cornerRadius, cornerRadius);
            p.setClipPath(path);
            p.drawPixmap(0, 0, 260, 146, thumbnail.scaled(260, 146, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            p.end();
            m_thumbnailLabel->setPixmap(rounded);
        } else {
            QPixmap placeholder(260, 146);
            placeholder.fill(Theme::popupColors().placeholder);
            QPainter p(&placeholder);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Theme::popupColors().dim);
            QFont f = font();
            f.setPointSize(10);
            p.setFont(f);
            p.drawText(QRectF(0, 0, 260, 146), Qt::AlignCenter, get_text("sidebar_no_preview"));
            p.end();
            m_thumbnailLabel->setPixmap(placeholder);
        }

        // Truthful state label. The previous implementation printed a value
        // derived from qHash(url) and called it "Memory usage", which was not
        // a measurement of anything.
        const bool sleeping = state == QLatin1String("sleeping");
        m_stateIcon->setText(sleeping ? QString::fromUtf8("\xF0\x9F\x92\xA4")
                                      : QString::fromUtf8("\xE2\x97\x8F"));
        m_stateLabel->setText(sleeping ? get_text("tab_sleeping_state")
                                       : state == QLatin1String("active")
                                             ? get_text("tab_active_state")
                                             : get_text("tab_background_state"));

        adjustSize();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        // `surface` is the card itself; `placeholder` is the empty-thumbnail box
        // drawn inside it. Conflating the two made the classic tooltip look like
        // a dark rectangle with a dark box on it.
        const Theme::PopupColors colors = Theme::popupColors();
        const bool classic = Theme::current() == Theme::ThemeStyle::Classic;
        p.setBrush(colors.surface);
        p.setPen(QPen(classic ? colors.border : QColor(255, 255, 255, 36), 1));
        const qreal radius = classic ? 3.0 : 12.0;
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    }

private:
    QLabel *m_titleLabel;
    QLabel *m_domainLabel;
    QLabel *m_thumbnailLabel;
    QLabel *m_stateIcon;
    QLabel *m_stateLabel;
};

class SidebarTabItemWidget : public QWidget
{
public:
    SidebarTabItemWidget(int tabIndex, const QIcon &icon, const QString &title,
                         const QUrl &url, bool isCurrent, bool expanded,
                         BrowserTabWidget *tabs, SidebarHoverCard *hoverCard,
                         BrowserSidebar *sidebar, QWidget *parent = nullptr)
        : QWidget(parent), m_index(tabIndex), m_title(title), m_url(url),
          m_tabs(tabs), m_hoverCard(hoverCard), m_sidebar(sidebar),
          m_expanded(expanded), m_isCurrent(isCurrent)
    {
        setAttribute(Qt::WA_Hover, true);
        // Hover feedback: the row can be dragged to reorder it or to tear the
        // tab off into its own window.
        setCursor(Qt::OpenHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(expanded ? QMargins(8, 4, 6, 4) : QMargins(4, 4, 4, 4));
        layout->setSpacing(6);

        m_iconLabel = new QLabel(this);
        m_iconLabel->setFixedSize(18, 18);
        m_iconLabel->setScaledContents(true);
        m_iconLabel->setPixmap(icon.pixmap(18, 18));
        m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        if (!expanded)
            layout->setAlignment(m_iconLabel, Qt::AlignCenter);
        layout->addWidget(m_iconLabel);

        m_titleLabel = new QLabel(title, this);
        const Theme::PopupColors colors = Theme::popupColors();
        m_titleLabel->setStyleSheet(isCurrent
            ? QStringLiteral("color: %1; font-weight: 600; font-size: 13px;").arg(colors.text.name())
            : QStringLiteral("color: %1; font-size: 13px;").arg(colors.dim.name()));
        m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        m_titleLabel->setVisible(expanded);
        layout->addWidget(m_titleLabel, 1);

        m_closeButton = new QToolButton(this);
        m_closeButton->setFixedSize(20, 20);
        // Painted, palette-following cross rather than the "✕" character: the
        // character inherits the button's colour, which the QSS sets, but its
        // weight and size differ per platform font and it reads as a text
        // glyph, not a button. This matches the tab strip's close button.
        m_closeButton->setIcon(Theme::icon(QStringLiteral("close"), 12));
        m_closeButton->setIconSize(QSize(12, 12));
        // The sidebar exists in both chromes, so the glyph has to survive a
        // light/dark flip without a rebuild of this row.
        connect(&ThemeManager::instance(), &ThemeManager::colorModeChanged, m_closeButton,
                [this](bool) {
                    if (m_closeButton)
                        m_closeButton->setIcon(Theme::icon(QStringLiteral("close"), 12));
                });
        m_closeButton->setToolTip(get_text("menu_close_tab"));
        m_closeButton->setCursor(Qt::PointingHandCursor);
        m_closeButton->setObjectName(QStringLiteral("SidebarTabCloseButton"));
        m_closeButton->setVisible(false);
        connect(m_closeButton, &QToolButton::clicked, this, [this]() {
            if (m_hoverCard)
                m_hoverCard->hide();
            if (m_tabs)
                m_tabs->closeTabAt(m_index);
        });
        layout->addWidget(m_closeButton);
    }

    void updateWidthMode(int currentWidth)
    {
        const bool showDetails = m_expanded && (currentWidth >= 120);
        m_titleLabel->setVisible(showDetails);
        m_closeButton->setVisible(showDetails && m_hovered);
        if (!showDetails) {
            layout()->setContentsMargins(4, 4, 4, 4);
            layout()->setAlignment(m_iconLabel, Qt::AlignCenter);
        } else {
            layout()->setContentsMargins(8, 4, 6, 4);
            layout()->setAlignment(m_iconLabel, Qt::AlignVCenter);
        }
    }

protected:
    void enterEvent(QEnterEvent *event) override
    {
        m_hovered = true;
        if (m_expanded) {
            m_closeButton->setVisible(true);
        } else {
            // Collapsed mode: show close button directly on the tab & display floating hover card with preview
            m_iconLabel->setVisible(false);
            m_closeButton->setFixedSize(26, 26);
            m_closeButton->setVisible(true);

            if (m_hoverCard && m_tabs) {
                const QPixmap thumbnail = m_tabs->tabThumbnail(m_index);
                const bool slept = m_tabs->isTabSlept(m_index);
                const bool active = (m_tabs->currentIndex() == m_index);
                // A state *code* crosses the boundary, not a translated
                // sentence: the card draws the label, and the sleeping check
                // below must not depend on which language that sentence is in.
                m_hoverCard->setTabInfo(m_title, m_url, thumbnail,
                                        slept ? QStringLiteral("sleeping")
                                              : active ? QStringLiteral("active")
                                                       : QStringLiteral("background"));
                const QPoint globalPos = mapToGlobal(QPoint(width() + 10, -6));
                m_hoverCard->move(globalPos);
                m_hoverCard->show();
            }
        }
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        m_hovered = false;
        if (m_expanded) {
            m_closeButton->setVisible(false);
        } else {
            m_iconLabel->setVisible(true);
            m_closeButton->setVisible(false);
            if (m_hoverCard)
                m_hoverCard->hide();
        }
        QWidget::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_tabs) {
            // Before the activation below, not after: setCurrentIndex() emits
            // currentTabChanged, which rebuilds every row, and this widget would
            // then be one of the rows scheduled for deletion — with the pointer
            // already pressed on it, so the drag that should follow would never
            // start. The flag makes that rebuild wait for the release.
            if (m_sidebar)
                m_sidebar->setRowPressActive(true);
            m_tabs->setCurrentIndex(m_index);
            // Remembered so mouseMoveEvent can tell a click apart from a drag:
            // activating a tab on press (Chrome does the same) must not turn a
            // slightly shaky click into a reorder.
            m_dragOrigin = event->position().toPoint();
            m_dragCandidate = true;
        } else if (event->button() == Qt::MiddleButton && m_tabs) {
            if (m_hoverCard)
                m_hoverCard->hide();
            m_tabs->closeTabAt(m_index);
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!m_dragCandidate || !m_tabs || !(event->buttons() & Qt::LeftButton)) {
            QWidget::mouseMoveEvent(event);
            return;
        }
        if ((event->position().toPoint() - m_dragOrigin).manhattanLength()
                < QApplication::startDragDistance()) {
            QWidget::mouseMoveEvent(event);
            return;
        }

        // One gesture per press. beginTabDrag() runs its own event loop and only
        // returns once the pointer is released, so clearing the flag first is
        // what keeps a single press from starting a second drag.
        m_dragCandidate = false;
        if (m_hoverCard)
            m_hoverCard->hide();
        if (m_sidebar)
            m_sidebar->beginTabDrag(m_index, this);

        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_dragCandidate = false;
        if (m_sidebar)
            m_sidebar->setRowPressActive(false);
        // The press deliberately postponed the row rebuilds it caused; this is
        // where they run, once nothing is holding on to the rows.
        if (m_sidebar)
            m_sidebar->flushDeferredRefresh();
        QWidget::mouseReleaseEvent(event);
    }

private:
    int m_index;
    QPoint m_dragOrigin;
    bool m_dragCandidate = false;
    QString m_title;
    QUrl m_url;
    BrowserTabWidget *m_tabs;
    SidebarHoverCard *m_hoverCard;
    // The list this row belongs to, so the drag can be handed to the widget that
    // owns the tab order. Not derived from parentWidget(): the row's parent is
    // the QListWidget's viewport, not the sidebar.
    BrowserSidebar *m_sidebar;
    bool m_expanded;
    bool m_isCurrent;
    bool m_hovered = false;
    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QToolButton *m_closeButton;
};
}

BrowserSidebar::BrowserSidebar(BrowserTabWidget *tabs, QWidget *parent)
    : QFrame(parent), m_tabs(tabs)
{
    setObjectName("BrowserSidebar");
    setFrameShape(QFrame::NoFrame);
    setMinimumWidth(kCollapsedWidth);
    setMaximumWidth(kExpandedMaxWidth);

    m_hoverCard = new SidebarHoverCard(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(2, 2, 2, 2);
    topRow->setSpacing(6);

    m_toggleButton = new QToolButton(this);
    m_toggleButton->setObjectName("SidebarToggleButton");
    m_toggleButton->setText("☰");
    m_toggleButton->setToolTip(get_text("nav_sidebar_expand"));
    m_toggleButton->setAutoRaise(true);
    m_toggleButton->setFixedSize(36, 32);
    topRow->addWidget(m_toggleButton);

    m_title = new QLabel(get_text("sidebar_tabs"), this);
    m_title->setObjectName("SidebarTitle");
    m_title->setVisible(false);
    topRow->addWidget(m_title);
    topRow->addStretch();
    layout->addLayout(topRow);

    // The drop targets a dragged row; see beginTabDrag(). Without this the
    // platform never offers this frame a drag at all.
    setAcceptDrops(true);
    m_dropIndicator = new QWidget(this);
    m_dropIndicator->setObjectName("SidebarDropIndicator");
    m_dropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_dropIndicator->hide();

    m_tabList = new QListWidget(this);
    m_tabList->setObjectName("SidebarTabs");
    // Rows start the drag themselves, in SidebarTabItemWidget::mouseMoveEvent().
    // Leaving the view's own drag support off is what keeps it from also trying
    // to insert rows into its model on a drop.
    m_tabList->setDragEnabled(false);
    m_tabList->setAcceptDrops(false);
    m_tabList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tabList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tabList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_tabList, 1);

    m_newTabButton = new QToolButton(this);
    m_newTabButton->setObjectName("SidebarNewTabButton");
    m_newTabButton->setText("+");
    m_newTabButton->setToolTip(get_text("file_new_tab"));
    m_newTabButton->setFixedHeight(36);
    m_newTabButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_newTabButton->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_newTabButton);

    connect(m_toggleButton, &QToolButton::clicked, this, [this] { setExpanded(!m_expanded); });
    connect(m_newTabButton, &QToolButton::clicked, m_tabs, [this] { m_tabs->addNewTab(); });
    connect(m_tabList, &QListWidget::currentRowChanged, m_tabs, [this](int row) {
        if (row >= 0 && row < m_tabs->count() && row != m_tabs->currentIndex())
            m_tabs->setCurrentIndex(row);
    });

    m_tabList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const int row = m_tabList->row(m_tabList->itemAt(pos));
        if (row >= 0 && row < m_tabs->count()) {
            if (m_hoverCard)
                m_hoverCard->hide();
            m_tabs->showTabContextMenu(row, m_tabList->mapToGlobal(pos));
        }
    });

    connect(m_tabs, &BrowserTabWidget::tabsChanged, this, [this] { refresh(); });
    connect(m_tabs, &BrowserTabWidget::tabIconChanged, this, [this](int, const QIcon &) { refresh(); });
    connect(m_tabs, &BrowserTabWidget::currentTabChanged, this, [this](int index) {
        // Highlight first, then rebuild: the rows carry the "current" styling
        // themselves (bold title), so switching tabs has to rebuild them.
        // refresh() is drag-safe — it defers while a row is being dragged.
        m_tabList->blockSignals(true);
        m_tabList->setCurrentRow(index);
        m_tabList->blockSignals(false);
        refresh();
    });

    setExpanded(false);
    refresh();
}

void BrowserSidebar::refresh()
{
    // Every tab event rebuilds the whole list from scratch, and doing that in
    // the middle of a drag would hand QListWidget a freshly built row widget
    // while the drag is still animating the old one — the drop target loses its
    // source. The rebuild is not skipped, only postponed until the gesture ends.
    // A press flag that outlived its press (a release delivered somewhere else,
    // a window that lost focus mid-gesture) would otherwise freeze the list
    // forever: nothing would ever rebuild it again. No button held means no
    // press to protect, whatever the flag says.
    if (m_rowPressActive && QApplication::mouseButtons() == Qt::NoButton)
        m_rowPressActive = false;

    if (m_dragActive || m_rowPressActive) {
        m_refreshDeferred = true;
        return;
    }
    m_refreshDeferred = false;

    const int current = m_tabs->currentIndex();
    m_tabList->blockSignals(true);
    m_tabList->clear();

    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *view = qobject_cast<QWebEngineView *>(m_tabs->widget(i));
        // A slept tab has no view; its title/URL live on the tab bar itself.
        const QString tabLabel = m_tabs->tabText(i);
        const QString title = view && !view->title().isEmpty() ? view->title()
                                                              : (tabLabel.isEmpty()
                                                                     ? get_text("new_tab_title")
                                                                     : tabLabel);
        const QUrl url = view ? view->url() : QUrl(m_tabs->urlForTabIndex(i));
        const QIcon icon = m_tabs->tabIcon(i);

        auto *item = new QListWidgetItem(m_tabList);
        item->setSizeHint(QSize(m_expanded ? 220 : 42, 38));

        auto *rowWidget = new SidebarTabItemWidget(i, icon, title, url, (i == current), m_expanded, m_tabs, dynamic_cast<SidebarHoverCard *>(m_hoverCard), this, m_tabList);
        m_tabList->setItemWidget(item, rowWidget);
    }
    m_tabList->setCurrentRow(current);
    m_tabList->blockSignals(false);
}

// ---------------------------------------------------------------------------
// Row drag and drop
//
// The gesture is a real QDrag so the platform draws the drag image and the
// pointer feedback users expect; the drop target is this frame, not the
// QListWidget. The list is deliberately left with drag/drop disabled: a
// QListWidget that accepts drops wants to insert model rows, and the rows here
// are a mirror of QTabWidget's tab order, not a model of their own. Accepting
// the drop here means the tab order is moved once, in the place it lives.
// ---------------------------------------------------------------------------

void BrowserSidebar::moveTab(int from, int to)
{
    // The order lives in QTabWidget, not in this list. QTabBar::moveTab() also
    // moves the page widget inside QTabWidget's stack, so there is nothing to
    // shuffle by hand — and no chance of the two orders drifting apart.
    if (!m_tabs || from == to)
        return;
    if (from < 0 || to < 0 || from >= m_tabs->count() || to >= m_tabs->count())
        return;
    m_tabs->tabBar()->moveTab(from, to);
    // The rows are a rendering of that order, and QTabWidget emits no signal for
    // a move, so nothing else would redraw them. refresh() defers itself while
    // the drag that caused this is still running (see its guard).
    refresh();
}

void BrowserSidebar::flushDeferredRefresh()
{
    if (!m_refreshDeferred || m_dragActive || m_rowPressActive)
        return;
    refresh(); // refresh() clears the flag itself
}

void BrowserSidebar::updateDropIndicator(const QPoint &sidebarPos)
{
    if (!m_dropIndicator)
        return;

    const QPoint inList = m_tabList->mapFrom(this, sidebarPos);
    const int gap = gapForPosition(m_tabList, inList.y());
    m_dropGap = gap;

    // The line is drawn in this frame's coordinates; the row rectangles are the
    // list's, so the one has to be mapped into the other.
    int yInList = 0;
    if (m_tabList->count() > 0) {
        if (gap < m_tabList->count())
            yInList = m_tabList->visualItemRect(m_tabList->item(gap)).top();
        else
            yInList = m_tabList->visualItemRect(m_tabList->item(m_tabList->count() - 1)).bottom();
    }
    const int y = m_tabList->mapTo(this, QPoint(0, yInList)).y();
    const int x = m_tabList->mapTo(this, QPoint(0, 0)).x();
    m_dropIndicator->setGeometry(x + 4, qMax(0, y - 1), qMax(0, m_tabList->width() - 8), 2);
    m_dropIndicator->show();
    m_dropIndicator->raise();
}

void BrowserSidebar::clearDropIndicator()
{
    m_dropGap = -1;
    if (m_dropIndicator)
        m_dropIndicator->hide();
}

void BrowserSidebar::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_dragActive && event->mimeData() && event->mimeData()->hasFormat(QString::fromLatin1(kTabMimeType))) {
        updateDropIndicator(event->position().toPoint());
        event->acceptProposedAction();
        return;
    }
    QFrame::dragEnterEvent(event);
}

void BrowserSidebar::dragMoveEvent(QDragMoveEvent *event)
{
    if (m_dragActive && event->mimeData() && event->mimeData()->hasFormat(QString::fromLatin1(kTabMimeType))) {
        updateDropIndicator(event->position().toPoint());
        event->acceptProposedAction();
        return;
    }
    QFrame::dragMoveEvent(event);
}

void BrowserSidebar::dragLeaveEvent(QDragLeaveEvent *event)
{
    clearDropIndicator();
    QFrame::dragLeaveEvent(event);
}

void BrowserSidebar::dropEvent(QDropEvent *event)
{
    if (!m_dragActive || !event->mimeData()
        || !event->mimeData()->hasFormat(QString::fromLatin1(kTabMimeType))) {
        QFrame::dropEvent(event);
        return;
    }

    const int from = event->mimeData()->data(QString::fromLatin1(kTabMimeType)).toInt();
    // Recompute rather than trusting the last dragMoveEvent: a drop can arrive
    // after the indicator was cleared by an intervening dragLeaveEvent.
    const QPoint inList = m_tabList->mapFrom(this, event->position().toPoint());
    const int gap = gapForPosition(m_tabList, inList.y());
    const int to = destinationForGap(gap, from);
    clearDropIndicator();
    event->acceptProposedAction();
    m_droppedInList = true;

    if (from >= 0 && from < m_tabs->count() && to != from)
        moveTab(from, to);
}

void BrowserSidebar::beginTabDrag(int index, QWidget *rowWidget)
{
    if (m_dragActive || !m_tabs || !rowWidget)
        return;
    if (index < 0 || index >= m_tabs->count())
        return;

    // Not m_refreshDeferred: the press that started this gesture almost
    // certainly asked for a rebuild (activating the tab does), and that is still
    // owed when the drag finishes.
    m_dragActive = true;
    m_rowPressActive = false;
    m_droppedInList = false;

    // The drag image is the row as it looks right now: grab() renders the live
    // widget, so what follows the cursor matches what is in the list.
    const QPixmap pixmap = rowWidget->grab();
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(kTabMimeType), QByteArray::number(index));

    auto *drag = new QDrag(rowWidget);
    drag->setMimeData(mime); // QDrag takes ownership
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));
    drag->exec(Qt::MoveAction);

    // Past this point the gesture is over: a drop inside the list already moved
    // the tab in dropEvent(), so the only remaining case is a release somewhere
    // this frame never saw.
    clearDropIndicator();
    m_dragActive = false;

    if (!m_droppedInList) {
        // Released over the page area, the toolbar or another application. Only
        // a release outside this window's frame is a tear-off; anything inside
        // it is a cancelled drag, which is what dropping a tab on its own page
        // has always meant in a browser.
        const QRect frame = window()->frameGeometry();
        if (!frame.contains(QCursor::pos())) {
            // Queued on purpose: the handler creates a window and, when this was
            // the last tab, closes this one — which would delete the sidebar
            // whose beginTabDrag() call is still on the stack.
            QMetaObject::invokeMethod(this, [this, index]() { emit tabTearOffRequested(index); },
                                      Qt::QueuedConnection);
        }
    }

    // Any rebuild that arrived during the gesture — including the one from the
    // reorder this drop performed — happens now, with nothing holding the rows.
    flushDeferredRefresh();
}

QSize BrowserSidebar::sizeHint() const
{
    return QSize(m_expanded ? 240 : kCollapsedWidth, 600);
}

QSize BrowserSidebar::minimumSizeHint() const
{
    return QSize(kCollapsedWidth, 100);
}

void BrowserSidebar::setExpanded(bool expanded)
{
    const bool changed = m_expanded != expanded;
    m_expanded = expanded;

    setProperty("compact", !expanded);
    style()->unpolish(this);
    style()->polish(this);

    m_toggleButton->setText(expanded ? "‹" : "☰");
    m_toggleButton->setToolTip(expanded ? get_text("nav_sidebar_collapse")
                                        : get_text("nav_sidebar_expand"));
    m_title->setVisible(expanded);
    m_newTabButton->setText(expanded ? QStringLiteral("+  ") + get_text("file_new_tab")
                                     : QStringLiteral("+"));
    m_newTabButton->setToolTip(get_text("file_new_tab"));

    updateGeometry();
    refresh();

    if (changed)
        emit expandedChanged(expanded);
}

void BrowserSidebar::retranslate()
{
    m_title->setText(get_text("sidebar_tabs"));
    m_toggleButton->setToolTip(m_expanded ? get_text("nav_sidebar_collapse")
                                          : get_text("nav_sidebar_expand"));
    m_newTabButton->setText(m_expanded ? QStringLiteral("+  ") + get_text("file_new_tab")
                                       : QStringLiteral("+"));
    m_newTabButton->setToolTip(get_text("file_new_tab"));
    // The per-row close buttons carry a tooltip too, and the rows are rebuilt
    // here rather than walking each item widget: refresh() is the same path the
    // tab list already uses for every other change.
    refresh();
}

void BrowserSidebar::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    const int w = width();
    const bool showDetails = m_expanded && (w >= 120);

    m_title->setVisible(showDetails);
    m_toggleButton->setText(m_expanded ? "‹" : "☰");
    m_toggleButton->setToolTip(m_expanded ? get_text("nav_sidebar_collapse")
                                          : get_text("nav_sidebar_expand"));

    if (showDetails) {
        m_newTabButton->setText(QStringLiteral("+  ") + get_text("file_new_tab"));
    } else {
        m_newTabButton->setText("+");
    }

    for (int i = 0; i < m_tabList->count(); ++i) {
        if (auto *item = m_tabList->item(i)) {
            if (auto *widget = dynamic_cast<SidebarTabItemWidget *>(m_tabList->itemWidget(item))) {
                widget->updateWidthMode(w);
            }
        }
    }
}
