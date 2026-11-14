#include "IdleEasterEgg.h"
#include "BrowserResources.h"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QPixmap>
#include <QVBoxLayout>
#include <QRandomGenerator>

EasterEggPopup::EasterEggPopup(const QString &imagePath, QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel *imgLabel = new QLabel(this);
    QPixmap pix(imagePath);
    if (!pix.isNull()) {
        if (pix.width() > 400 || pix.height() > 400) {
            pix = pix.scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        imgLabel->setPixmap(pix);
    }
    imgLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(imgLabel);

    resize(imgLabel->sizeHint());
}

void EasterEggPopup::mousePressEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    close();
}

void EasterEggPopup::keyPressEvent(QKeyEvent *event) {
    Q_UNUSED(event);
    close();
}

IdleEasterEgg::IdleEasterEgg(QMainWindow *window, QObject *parent)
    : QObject(parent), m_window(window) {
    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    connect(m_idleTimer, &QTimer::timeout, this, &IdleEasterEgg::onIdleTimeout);
    m_idleTimer->start(m_idleThresholdMs);

    qApp->installEventFilter(this);
}

IdleEasterEgg::~IdleEasterEgg() {
    cleanup();
}

void IdleEasterEgg::cleanup() {
    if (m_currentPopup) {
        m_currentPopup->close();
        m_currentPopup = nullptr;
    }
    if (m_idleTimer) {
        m_idleTimer->stop();
    }
}

bool IdleEasterEgg::eventFilter(QObject *obj, QEvent *event) {
    switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::KeyPress:
        case QEvent::Wheel:
            if (m_currentPopup) {
                m_currentPopup->close();
                m_currentPopup = nullptr;
            }
            if (m_idleTimer) {
                m_idleTimer->start(m_idleThresholdMs);
            }
            break;
        default:
            break;
    }
    return QObject::eventFilter(obj, event);
}

void IdleEasterEgg::onIdleTimeout() {
    showEasterEgg();
}

void IdleEasterEgg::showEasterEgg() {
    if (m_currentPopup) return;

    QString eggDir = BrowserResources::getAppRootDir() + "/Easter_Egg";
    QDir dir(eggDir);
    if (!dir.exists()) return;

    QStringList imgFilters = {"*.png", "*.jpg", "*.jpeg", "*.webp"};
    QStringList files = dir.entryList(imgFilters, QDir::Files);
    if (files.isEmpty()) return;

    int idx = QRandomGenerator::global()->bounded(files.size());
    QString chosen = dir.filePath(files[idx]);

    m_currentPopup = new EasterEggPopup(chosen, (QWidget*)m_window);
    m_currentPopup->show();
}
