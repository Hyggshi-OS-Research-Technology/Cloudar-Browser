#include "BrowserSidebar.h"
#include "BrowserTabWidget.h"
#include "ClassicTheme.h"
#include "Language.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QToolButton>
#include <QWebEngineView>
#include <QStyle>
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
        divider->setStyleSheet(QStringLiteral("background: rgba(255, 255, 255, 0.08); max-height: 1px; min-height: 1px; border: none;"));
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
                         QWidget *parent = nullptr)
        : QWidget(parent), m_index(tabIndex), m_title(title), m_url(url),
          m_tabs(tabs), m_hoverCard(hoverCard), m_expanded(expanded), m_isCurrent(isCurrent)
    {
        setAttribute(Qt::WA_Hover, true);
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
        m_closeButton->setText(QStringLiteral("✕"));
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
            m_tabs->setCurrentIndex(m_index);
        } else if (event->button() == Qt::MiddleButton && m_tabs) {
            if (m_hoverCard)
                m_hoverCard->hide();
            m_tabs->closeTabAt(m_index);
        }
        QWidget::mousePressEvent(event);
    }

private:
    int m_index;
    QString m_title;
    QUrl m_url;
    BrowserTabWidget *m_tabs;
    SidebarHoverCard *m_hoverCard;
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

    m_tabList = new QListWidget(this);
    m_tabList->setObjectName("SidebarTabs");
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

        auto *rowWidget = new SidebarTabItemWidget(i, icon, title, url, (i == current), m_expanded, m_tabs, dynamic_cast<SidebarHoverCard *>(m_hoverCard), m_tabList);
        m_tabList->setItemWidget(item, rowWidget);
    }
    m_tabList->setCurrentRow(current);
    m_tabList->blockSignals(false);
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
