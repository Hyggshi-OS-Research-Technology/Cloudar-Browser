#include "NotificationToast.h"

#include "ClassicTheme.h"
#include "ThemeManager.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWebEngineNotification>

NotificationToast::NotificationToast(QWidget *parent) : QFrame(parent)
{
    // The rounded corners come from paintEvent(), so the widget itself must not
    // paint a rectangle behind them.
    setAttribute(Qt::WA_TranslucentBackground, true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
}

void NotificationToast::setNotification(std::shared_ptr<QWebEngineNotification> notification)
{
    m_notification = std::move(notification);
}

QWebEngineNotification *NotificationToast::notification() const
{
    return m_notification.get();
}

void NotificationToast::dismiss()
{
    if (m_dismissed)
        return;
    m_dismissed = true;
    // close() first, then close the card: the page's close event is the part
    // that matters if the process is about to lose this widget.
    if (m_notification)
        m_notification->close();
    close();
}

void NotificationToast::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const ThemeManager::Palette pal = ThemeManager::instance().palette();
    const bool classic = Theme::current() == Theme::ThemeStyle::Classic;
    // A Win32 tooltip has near-square corners; the modern chrome rounds.
    const qreal radius = classic ? 3.0 : 10.0;
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    // A subtle shadow stand-in: a darker rounded rect offset by one pixel. A
    // real QGraphicsDropShadowEffect would mean a render-to-texture per toast,
    // and the Legacy CPU tier is exactly where that hurts.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 38));
    p.drawRoundedRect(box.translated(0, 1), radius, radius);

    QPainterPath path;
    path.addRoundedRect(box, radius, radius);
    p.setBrush(pal.chrome);
    p.setPen(QPen(pal.border, 1));
    p.drawPath(path);
}

void NotificationToast::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_pressed = true;
    QFrame::mousePressEvent(event);
}

void NotificationToast::mouseReleaseEvent(QMouseEvent *event)
{
    const bool activate = m_pressed && event->button() == Qt::LeftButton
                          && rect().contains(event->pos());
    m_pressed = false;
    if (activate) {
        // The page's click() handler: this is what tells the site the user acted
        // on the notification, and on most sites it is the notification click
        // that focuses the relevant thread. The card then goes away, which also
        // fires close() — the same sequence Chrome runs.
        if (m_notification)
            m_notification->click();
        emit activated();
        dismiss();
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}
