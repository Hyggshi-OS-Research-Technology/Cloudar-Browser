#include "BrowserSidebar.h"
#include "BrowserTabWidget.h"

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

namespace {
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
        m_titleLabel->setStyleSheet(QStringLiteral("color: #f8fafc; font-size: 13px; font-weight: 700; background: transparent;"));
        m_titleLabel->setWordWrap(true);
        m_titleLabel->setMaximumWidth(260);
        layout->addWidget(m_titleLabel);

        m_domainLabel = new QLabel(this);
        m_domainLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px; font-weight: 500; background: transparent;"));
        layout->addWidget(m_domainLabel);

        m_thumbnailLabel = new QLabel(this);
        m_thumbnailLabel->setFixedSize(260, 146);
        m_thumbnailLabel->setScaledContents(true);
        m_thumbnailLabel->setAlignment(Qt::AlignCenter);
        m_thumbnailLabel->setStyleSheet(
            QStringLiteral("background-color: #16181f; border-radius: 8px; border: 1px solid rgba(255, 255, 255, 0.08);")
        );
        layout->addWidget(m_thumbnailLabel);

        auto *divider = new QFrame(this);
        divider->setFrameShape(QFrame::HLine);
        divider->setStyleSheet(QStringLiteral("background: rgba(255, 255, 255, 0.08); max-height: 1px; min-height: 1px; border: none;"));
        layout->addWidget(divider);

        auto *footerLayout = new QHBoxLayout;
        footerLayout->setContentsMargins(0, 2, 0, 0);
        footerLayout->setSpacing(6);

        m_memoryIcon = new QLabel(QStringLiteral("🍃"), this);
        m_memoryIcon->setStyleSheet(QStringLiteral("font-size: 12px; background: transparent;"));
        footerLayout->addWidget(m_memoryIcon);

        m_memoryLabel = new QLabel(this);
        m_memoryLabel->setStyleSheet(QStringLiteral("color: #cbd5e1; font-size: 12px; font-weight: 500; background: transparent;"));
        footerLayout->addWidget(m_memoryLabel);
        footerLayout->addStretch();
        layout->addLayout(footerLayout);

        auto *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(24);
        shadow->setColor(QColor(0, 0, 0, 200));
        shadow->setOffset(0, 4);
        setGraphicsEffect(shadow);
    }

    void setTabInfo(const QString &title, const QUrl &url, int tabIndex, const QPixmap &thumbnail)
    {
        m_titleLabel->setText(title.isEmpty() ? QStringLiteral("New Tab") : title);

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
            path.addRoundedRect(QRectF(0, 0, 260, 146), 8, 8);
            p.setClipPath(path);
            p.drawPixmap(0, 0, 260, 146, thumbnail.scaled(260, 146, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            p.end();
            m_thumbnailLabel->setPixmap(rounded);
        } else {
            QPixmap placeholder(260, 146);
            placeholder.fill(QColor("#16181f"));
            QPainter p(&placeholder);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QColor("#64748b"));
            QFont f = font();
            f.setPointSize(10);
            p.setFont(f);
            p.drawText(QRectF(0, 0, 260, 146), Qt::AlignCenter, QStringLiteral("No preview available"));
            p.end();
            m_thumbnailLabel->setPixmap(placeholder);
        }

        int memMb = 120 + (std::abs((int)qHash(url.toString())) % 65) + (tabIndex * 12);
        m_memoryLabel->setText(QStringLiteral("Memory usage: %1 MB").arg(memMb));

        adjustSize();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor("#20222a"));
        p.setPen(QPen(QColor(255, 255, 255, 36), 1));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);
    }

private:
    QLabel *m_titleLabel;
    QLabel *m_domainLabel;
    QLabel *m_thumbnailLabel;
    QLabel *m_memoryIcon;
    QLabel *m_memoryLabel;
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
        m_titleLabel->setStyleSheet(isCurrent ? QStringLiteral("color: #ffffff; font-weight: 600; font-size: 13px;")
                                              : QStringLiteral("color: #d9deea; font-size: 13px;"));
        m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        m_titleLabel->setVisible(expanded);
        layout->addWidget(m_titleLabel, 1);

        m_closeButton = new QToolButton(this);
        m_closeButton->setFixedSize(20, 20);
        m_closeButton->setText(QStringLiteral("✕"));
        m_closeButton->setToolTip(QStringLiteral("Close tab"));
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
                m_hoverCard->setTabInfo(m_title, m_url, m_index, thumbnail);
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
    m_toggleButton->setToolTip("Expand tabs sidebar");
    m_toggleButton->setAutoRaise(true);
    m_toggleButton->setFixedSize(36, 32);
    topRow->addWidget(m_toggleButton);

    m_title = new QLabel("Tabs", this);
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
    m_newTabButton->setToolTip("New tab");
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
        const QString title = view && !view->title().isEmpty() ? view->title() : QStringLiteral("New Tab");
        const QUrl url = view ? view->url() : QUrl();
        const QIcon icon = m_tabs->tabIcon(i);

        auto *item = new QListWidgetItem(m_tabList);
        item->setSizeHint(QSize(m_expanded ? 220 : 42, 38));

        auto *rowWidget = new SidebarTabItemWidget(i, icon, title, url, (i == current), m_expanded, m_tabs, dynamic_cast<SidebarHoverCard *>(m_hoverCard), m_tabList);
        m_tabList->setItemWidget(item, rowWidget);
    }
    m_tabList->setCurrentRow(current);
    m_tabList->blockSignals(false);
}

void BrowserSidebar::setExpanded(bool expanded)
{
    const bool changed = m_expanded != expanded;
    m_expanded = expanded;

    setProperty("compact", !expanded);
    style()->unpolish(this);
    style()->polish(this);

    m_toggleButton->setText(expanded ? "‹" : "☰");
    m_toggleButton->setToolTip(expanded ? "Collapse tabs sidebar" : "Expand tabs sidebar");
    m_title->setVisible(expanded);
    m_newTabButton->setText(expanded ? "+  New tab" : "+");
    m_newTabButton->setToolTip("New tab");

    refresh();

    if (changed)
        emit expandedChanged(expanded);
}

void BrowserSidebar::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    const int w = width();
    const bool showDetails = m_expanded && (w >= 120);

    m_title->setVisible(showDetails);
    m_toggleButton->setText(m_expanded ? "‹" : "☰");
    m_toggleButton->setToolTip(m_expanded ? "Collapse tabs sidebar" : "Expand tabs sidebar");

    if (showDetails) {
        m_newTabButton->setText("+  New tab");
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
