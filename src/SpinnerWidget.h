#pragma once

#include <QWidget>
#include <QTimer>

// A tiny custom-painted rotating arc spinner, shown in the toolbar while a
// page is loading. Cheaper than a QMovie/GIF and needs no extra assets.
class SpinnerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SpinnerWidget(QWidget *parent = nullptr);

    void start();
    void stop();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTimer m_timer;
    int m_angle = 0;
};
