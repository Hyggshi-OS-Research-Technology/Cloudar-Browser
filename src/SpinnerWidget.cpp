#include "SpinnerWidget.h"

#include <QPainter>
#include <QConicalGradient>
#include <QPen>

SpinnerWidget::SpinnerWidget(QWidget *parent) : QWidget(parent)
{
    setFixedSize(18, 18);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        m_angle = (m_angle + 30) % 360;
        update();
    });
    hide();
}

void SpinnerWidget::start()
{
    m_angle = 0;
    show();
    m_timer.start(60);
}

void SpinnerWidget::stop()
{
    m_timer.stop();
    hide();
}

void SpinnerWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF rect(2, 2, width() - 4, height() - 4);

    QPen pen(QColor(45, 127, 249), 2.2);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    // Draw a 270-degree arc rotated by m_angle to fake continuous spinning.
    painter.drawArc(rect, m_angle * 16, 270 * 16);
}
