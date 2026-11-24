#include "TitleBar.h"
#include "ThemeManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMouseEvent>
#include <QMenu>
#include <QWindow>
#include <QIcon>

TitleBar::TitleBar(QWidget *parent) : QWidget(parent)
{
    setObjectName("TitleBar");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(36);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 6, 0);
    layout->setSpacing(8);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setPixmap(QIcon(":/icon.png").pixmap(20, 20));
    layout->addWidget(m_iconLabel);

    m_titleLabel = new QLabel("Cloudar Browser", this);
    m_titleLabel->setStyleSheet("font-weight: 600; background: transparent;");
    layout->addWidget(m_titleLabel);
    layout->addStretch();

    // Chrome's three-dot menu button, immediately left of the window controls.
    // InstantPopup means the menu opens on press and the button never keeps a
    // "checked" look, which is how Chrome behaves.
    m_menuButton = new QToolButton(this);
    m_menuButton->setObjectName("TitleBarMenuButton");
    m_menuButton->setText(QString::fromUtf8("\xE2\x8B\xAE")); // ⋮
    m_menuButton->setToolTip(tr("Customise and control Cloudar Browser"));
    m_menuButton->setAutoRaise(true);
    m_menuButton->setFixedSize(30, 26);
    m_menuButton->setCursor(Qt::ArrowCursor);
    m_menuButton->setPopupMode(QToolButton::InstantPopup);
    layout->addWidget(m_menuButton);

    auto makeButton = [this](const QString &text) {
        auto *btn = new QToolButton(this);
        btn->setText(text);
        btn->setAutoRaise(true);
        btn->setFixedSize(30, 26);
        btn->setCursor(Qt::ArrowCursor);
        return btn;
    };

    m_minBtn = makeButton(QString::fromUtf8("\xE2\x88\x92"));   // −
    m_maxBtn = makeButton(QString::fromUtf8("\xE2\x96\xA1"));   // □
    m_closeBtn = makeButton(QString::fromUtf8("\xE2\x9C\x95")); // ✕
    m_closeBtn->setObjectName("TitleBarCloseButton");

    connect(m_minBtn, &QToolButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maxBtn, &QToolButton::clicked, this, &TitleBar::maximizeRestoreRequested);
    connect(m_closeBtn, &QToolButton::clicked, this, &TitleBar::closeRequested);

    layout->addWidget(m_minBtn);
    layout->addWidget(m_maxBtn);
    layout->addWidget(m_closeBtn);

    applyTheme(ThemeManager::instance().accentColor());
    connect(&ThemeManager::instance(), &ThemeManager::accentChanged, this, &TitleBar::applyTheme);
}

void TitleBar::setTitle(const QString &title)
{
    m_titleLabel->setText(title.isEmpty() ? "LightBrowser" : title);
}

void TitleBar::setMenu(QMenu *menu)
{
    if (!m_menuButton)
        return;
    m_menuButton->setMenu(menu);
    m_menuButton->setVisible(menu != nullptr);
}

void TitleBar::applyTheme(const QColor &accent)
{
    const QColor text = ThemeManager::readableTextOn(accent);
    const QColor hover = accent.lighter(115);

    setStyleSheet(QString(
        "#TitleBar { background: %1; }"
        "#TitleBar QLabel { color: %2; background: transparent; }"
        "#TitleBar QToolButton { color: %2; border: none; border-radius: 4px; font-weight: bold; }"
        "#TitleBar QToolButton:hover { background: %3; }"
        "#TitleBarCloseButton:hover { background: #e5484d; color: white; }"
        // The three-dot glyph is a text character, so it needs a font that
        // actually has U+22EE; without this it renders as a box on some systems.
        "#TitleBarMenuButton { font-size: 17px; font-weight: normal; padding-bottom: 6px; }"
        "#TitleBarMenuButton::menu-indicator { image: none; width: 0px; }"
    ).arg(accent.name(), text.name(), hover.name()));
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (auto *handle = window()->windowHandle()) {
            handle->startSystemMove();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit maximizeRestoreRequested();
    QWidget::mouseDoubleClickEvent(event);
}
