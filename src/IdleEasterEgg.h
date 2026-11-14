#ifndef IDLE_EASTER_EGG_H
#define IDLE_EASTER_EGG_H

#include <QObject>
#include <QTimer>
#include <QDialog>
#include <QLabel>

class QMainWindow;

class EasterEggPopup : public QDialog {
    Q_OBJECT

public:
    explicit EasterEggPopup(const QString &imagePath, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

class IdleEasterEgg : public QObject {
    Q_OBJECT

public:
    explicit IdleEasterEgg(QMainWindow *window, QObject *parent = nullptr);
    ~IdleEasterEgg() override;

    void cleanup();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onIdleTimeout();

private:
    void showEasterEgg();

    QMainWindow *m_window = nullptr;
    QTimer *m_idleTimer = nullptr;
    EasterEggPopup *m_currentPopup = nullptr;
    qint64 m_idleThresholdMs = 5 * 60 * 1000; // 5 minutes
};

#endif // IDLE_EASTER_EGG_H
