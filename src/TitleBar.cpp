#include "TitleBar.h"
#include "ThemeManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMouseEvent>
#include <QMenu>
#include <QWindow>
#include <QIcon>
#include <QPainter>
#include <QPen>
#include <QPixmap>

namespace {

// Three dots, drawn. The first version used the text character U+22EE, which
// rendered small and pale in the title bar — and would have been a missing-glyph
// box on any system whose UI font lacks it. Painting them keeps the size, the
// spacing and the colour under our control and cannot depend on a font at all.
QIcon makeMenuGlyph(const QColor &color, int size)
{
    // Drawn at 3x and scaled down by QIcon so the dots stay round on HiDPI.
    constexpr int kScale = 3;
    QPixmap pix(size * kScale, size * kScale);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(color);

    const qreal s = size * kScale;
    // Chrome's dots are roughly 2 px across in an 18 px box, with a 5 px pitch.
    // The first version used a 3 px box and a 4.3 px pitch, which at title-bar
    // scale read as a faint smudge rather than a button.
    const qreal radius = s * 0.115;
    const qreal cx = s / 2.0;
    const qreal first = s * 0.24;
    const qreal step = s * 0.26;
    for (int i = 0; i < 3; ++i)
        p.drawEllipse(QPointF(cx, first + step * i), radius, radius);
    p.end();

    QPixmap disabled(pix.size());
    disabled.fill(Qt::transparent);
    {
        QPainter dp(&disabled);
        dp.setRenderHint(QPainter::Antialiasing, true);
        dp.setPen(Qt::NoPen);
        dp.setBrush(QColor(color.red(), color.green(), color.blue(), 90));
        for (int i = 0; i < 3; ++i)
            dp.drawEllipse(QPointF(cx, first + step * i), radius, radius);
    }

    QIcon icon;
    icon.addPixmap(pix, QIcon::Normal);
    icon.addPixmap(disabled, QIcon::Disabled);
    return icon;
}

// The window buttons were text characters too: a minus, a square and a
// multiplication sign. They are simple shapes, so painting them removes the font
// dependency and the odd baseline that made the box glyph sit too low.
void paintWindowShape(QPainter &p, const QString &shape, int size)
{
    const qreal s = size;
    const qreal c = s / 2.0;
    const qreal arm = s * 0.22;

    QPen pen = p.pen();
    pen.setWidthF(qMax(1.0, s * 0.055));
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    if (shape == QLatin1String("min")) {
        p.drawLine(QPointF(c - arm, c), QPointF(c + arm, c));
    } else if (shape == QLatin1String("max")) {
        const qreal r = arm * 0.85;
        p.drawRect(QRectF(c - r, c - r, r * 2, r * 2));
    } else { // close
        p.drawLine(QPointF(c - arm, c - arm), QPointF(c + arm, c + arm));
        p.drawLine(QPointF(c + arm, c - arm), QPointF(c - arm, c + arm));
    }
}

QPixmap renderWindowGlyph(const QString &shape, const QColor &color, int size)
{
    constexpr int kScale = 3;
    QPixmap pix(size * kScale, size * kScale);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(color);
    paintWindowShape(p, shape, size * kScale);
    p.end();
    return pix;
}

QIcon makeWindowGlyph(const QString &shape, const QColor &color, int size,
                      const QColor &activeColor = QColor())
{
    QIcon icon;
    icon.addPixmap(renderWindowGlyph(shape, color, size), QIcon::Normal);

    // The close button turns red on hover and needs a white glyph there, while
    // the others keep their normal colour. QIcon::Active is the mode QToolButton
    // uses under the mouse, so the two states can differ.
    if (activeColor.isValid())
        icon.addPixmap(renderWindowGlyph(shape, activeColor, size), QIcon::Active);
    return icon;
}

} // namespace

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
    m_menuButton->setIconSize(QSize(20, 20));
    m_menuButton->setToolTip(tr("Customise and control Cloudar Browser"));
    m_menuButton->setAutoRaise(true);
    m_menuButton->setFixedSize(30, 26);
    m_menuButton->setCursor(Qt::ArrowCursor);
    m_menuButton->setPopupMode(QToolButton::InstantPopup);
    layout->addWidget(m_menuButton);

    auto makeButton = [this](const QString &shape) {
        auto *btn = new QToolButton(this);
        btn->setObjectName("TitleBarWindowButton");
        // The shape name rides along as a property so applyTheme() can repaint
        // the glyph in the new accent-derived colour.
        btn->setProperty("glyphShape", shape);
        btn->setIconSize(QSize(12, 12));
        btn->setAutoRaise(true);
        btn->setFixedSize(30, 26);
        btn->setCursor(Qt::ArrowCursor);
        return btn;
    };

    m_minBtn = makeButton(QStringLiteral("min"));
    m_maxBtn = makeButton(QStringLiteral("max"));
    m_closeBtn = makeButton(QStringLiteral("close"));
    m_closeBtn->setObjectName("TitleBarCloseButton");

    connect(m_minBtn, &QToolButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maxBtn, &QToolButton::clicked, this, &TitleBar::maximizeRestoreRequested);
    connect(m_closeBtn, &QToolButton::clicked, this, &TitleBar::closeRequested);

    layout->addWidget(m_minBtn);
    layout->addWidget(m_maxBtn);
    layout->addWidget(m_closeBtn);

    applyTheme(ThemeManager::instance().accentColor());
    connect(&ThemeManager::instance(), &ThemeManager::accentChanged, this, &TitleBar::applyTheme);
    // The window controls are painted, so a light/dark flip has to *repaint*
    // them: applyTheme() derives their colour from readableTextOn(accent), and
    // in light mode that is dark glyphs on a light accent — the opposite of the
    // dark-mode answer. Without this the title bar kept the old glyphs until the
    // user happened to change the accent colour.
    connect(&ThemeManager::instance(), &ThemeManager::colorModeChanged, this,
            [this](bool) { applyTheme(ThemeManager::instance().accentColor()); });
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
        "#TitleBarMenuButton::menu-indicator { image: none; width: 0px; }"
    ).arg(accent.name(), text.name(), hover.name()));

    // Every glyph here is painted, so all of them have to be regenerated when
    // the accent — and therefore the readable text colour — changes.
    if (m_menuButton)
        m_menuButton->setIcon(makeMenuGlyph(text, 20));
    for (QToolButton *btn : {m_minBtn, m_maxBtn, m_closeBtn}) {
        if (!btn)
            continue;
        // Only the close button changes colour on hover (red background), so
        // only it needs a second, white pixmap for the Active mode.
        const QColor activeColor = (btn == m_closeBtn) ? QColor(Qt::white) : QColor();
        btn->setIcon(makeWindowGlyph(btn->property("glyphShape").toString(),
                                     text, 12, activeColor));
    }
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
