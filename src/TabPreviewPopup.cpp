#include "TabPreviewPopup.h"
#include "ClassicTheme.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

TabPreviewPopup::TabPreviewPopup(QWidget *parent) : QWidget(parent)
{
    // Use a sub-widget (QFrame) so QGraphicsOpacityEffect can be applied to it
    // without triggering the "plugin does not support setting window opacity"
    // warning that fires when animating the top-level window directly via
    // setWindowOpacity() on xcb without a compositing manager.
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);

    // Outer transparent container just to host the effect
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_card = new QFrame(this);
    m_card->setObjectName("previewCard");
    m_card->setStyleSheet(QStringLiteral("QFrame#previewCard { background: transparent; }"));
    outerLayout->addWidget(m_card);

    m_opacityEffect = new QGraphicsOpacityEffect(m_card);
    m_opacityEffect->setOpacity(0.0);
    m_card->setGraphicsEffect(m_opacityEffect);

    m_fadeAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeAnimation->setDuration(150);
    m_fadeAnimation->setEasingCurve(QEasingCurve::OutCubic);

    auto *layout = new QVBoxLayout(m_card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    m_titleLabel = new QLabel(m_card);
    // Colours come from Theme::popupColors() rather than being hard-coded: this
    // card floats over the page, so in the classic chrome it has to look like a
    // Win32 tooltip, not like the modern dark card.
    const Theme::PopupColors colors = Theme::popupColors();
    m_titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 700; background: transparent;")
                                    .arg(colors.text.name()));
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setMaximumWidth(260);
    layout->addWidget(m_titleLabel);

    m_domainLabel = new QLabel(m_card);
    m_domainLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 500; background: transparent;")
                                     .arg(colors.dim.name()));
    layout->addWidget(m_domainLabel);

    m_thumbnailLabel = new QLabel(m_card);
    m_thumbnailLabel->setFixedSize(260, 146);
    m_thumbnailLabel->setScaledContents(false);
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setStyleSheet(
        QStringLiteral("background-color: %1; border-radius: 4px; border: 1px solid %2;")
            .arg(colors.placeholder.name(), colors.border.name())
    );
    layout->addWidget(m_thumbnailLabel);

    auto *divider = new QFrame(m_card);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("background: rgba(255, 255, 255, 0.08); max-height: 1px; min-height: 1px; border: none;"));
    layout->addWidget(divider);

    auto *footerLayout = new QHBoxLayout;
    footerLayout->setContentsMargins(0, 2, 0, 0);
    footerLayout->setSpacing(6);

    m_stateIcon = new QLabel(m_card);
    m_stateIcon->setStyleSheet(QStringLiteral("font-size: 12px; background: transparent;"));
    footerLayout->addWidget(m_stateIcon);

    m_stateLabel = new QLabel(m_card);
    m_stateLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 500; background: transparent;")
                                    .arg(colors.dim.name()));
    footerLayout->addWidget(m_stateLabel);
    footerLayout->addStretch();
    layout->addLayout(footerLayout);
}

void TabPreviewPopup::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // This is the card outline itself (the thumbnail label paints inside it), so
    // it uses `surface`, not `placeholder`. The classic chrome wants the nearly
    // square 3 px radius of a Win32 tooltip; the modern chrome wants 12 px.
    const Theme::PopupColors colors = Theme::popupColors();
    const bool classic = Theme::current() == Theme::ThemeStyle::Classic;
    p.setBrush(colors.surface);
    p.setPen(QPen(classic ? colors.border : QColor(255, 255, 255, 36), 1));
    const qreal radius = classic ? 3.0 : 12.0;
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
}

void TabPreviewPopup::fadeIn()
{
    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(m_opacityEffect->opacity());
    m_fadeAnimation->setEndValue(1.0);
    show();
    m_fadeAnimation->start();
}

void TabPreviewPopup::fadeOut()
{
    if (!isVisible())
        return;
    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(m_opacityEffect->opacity());
    m_fadeAnimation->setEndValue(0.0);
    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_opacityEffect->opacity() == 0.0)
            hide();
    }, Qt::SingleShotConnection);
    m_fadeAnimation->start();
}

void TabPreviewPopup::setContent(const QPixmap &thumbnail, const QString &title, const QUrl &url,
                                  const QString &state)
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
        p.drawText(QRectF(0, 0, 260, 146), Qt::AlignCenter, QStringLiteral("No preview available"));
        p.end();
        m_thumbnailLabel->setPixmap(placeholder);
    }

    // No invented numbers: report only the state we can actually observe.
    const bool sleeping = state.startsWith(QStringLiteral("Sleeping"), Qt::CaseInsensitive);
    m_stateIcon->setText(sleeping ? QString::fromUtf8("\xF0\x9F\x92\xA4")   // 💤
                                   : QString::fromUtf8("\xE2\x97\x8F"));  // ●
    m_stateLabel->setText(state.isEmpty() ? QStringLiteral("Background") : state);

    adjustSize();
}
