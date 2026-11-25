#include "NotificationPresenter.h"

#include "NotificationToast.h"
#include "ClassicTheme.h"
#include "Language.h"
#include "PermissionStore.h"
#include "ThemeManager.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWebEngineNotification>
#include <QWebEngineProfile>

namespace {

// Three is where Chrome stops stacking them on screen. More than that and the
// corner of the window is a wall of text over the page.
constexpr int kMaxVisible = 3;
// Long enough to read a two-line message, short enough not to nag.
constexpr int kLifetimeMs = 6000;
constexpr int kToastWidth = 320;
constexpr int kMinToastHeight = 84;
constexpr int kMargin = 16;
constexpr int kSlotGap = 8;
// A page controls its message length; this is already more than fits.
constexpr int kMaxMessageChars = 240;

} // namespace

int NotificationPresenter::maxVisible()
{
    return kMaxVisible;
}

NotificationPresenter::NotificationPresenter(QObject *parent) : QObject(parent) {}

void NotificationPresenter::installOn(QWebEngineProfile *profile)
{
    if (!profile)
        return;
    // setNotificationPresenter() takes a std::function that *owns* the
    // notification (std::unique_ptr), so the presenter has to outlive the
    // callback. `this` is owned by the window and the window owns the profile,
    // so that holds for as long as the profile can deliver anything.
    profile->setNotificationPresenter(
        [this](std::unique_ptr<QWebEngineNotification> notification) {
            handle(std::move(notification));
        });
}

void NotificationPresenter::setHost(QWidget *host)
{
    m_host = host;
}

void NotificationPresenter::setActivateHandler(std::function<void(const QUrl &origin)> handler)
{
    m_activate = std::move(handler);
}

void NotificationPresenter::restyle()
{
    for (NotificationToast *toast : m_toasts) {
        if (toast)
            toast->update();
    }
}

void NotificationPresenter::handle(std::unique_ptr<QWebEngineNotification> notification)
{
    if (!notification)
        return;
    const QUrl origin = notification->origin();
    const QString host = origin.host();

    // A notification is a page-level capability, so the *stored permission
    // decision* is what decides whether it exists at all — the same gate as
    // camera and microphone, decided by the same prompt. A denied site is
    // dropped silently: the page must not be able to distinguish "denied" from
    // "nobody is looking at the screen".
    const QString decision = PermissionStore::decisionFor(host, QStringLiteral("notifications"));
    if (decision != QLatin1String("allow")) {
        // Also covers "ask": with no decision there is no consent to display
        // anything, and the notification would be a permission prompt the user
        // never saw.
        notification->close();
        return;
    }

    if (!m_host)
        return; // nowhere to draw it; close() above is the honest answer

    evictOldest();

    auto shared = std::shared_ptr<QWebEngineNotification>(notification.release());

    auto *toast = new NotificationToast(m_host);
    toast->setObjectName(QStringLiteral("NotificationToast"));
    toast->setAttribute(Qt::WA_DeleteOnClose, true);
    toast->setFixedWidth(kToastWidth);
    toast->setNotification(shared);

    const ThemeManager::Palette pal = ThemeManager::instance().palette();

    auto *layout = new QVBoxLayout(toast);
    layout->setContentsMargins(12, 10, 10, 10);
    layout->setSpacing(2);

    auto *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);

    // The page's own icon, or a drawn page glyph when it sent none.
    auto *icon = new QLabel(toast);
    icon->setFixedSize(18, 18);
    const QImage siteIcon = shared->icon();
    if (!siteIcon.isNull()) {
        icon->setPixmap(QPixmap::fromImage(
            siteIcon.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    } else {
        icon->setPixmap(Theme::icon(QStringLiteral("page"), 18).pixmap(18, 18));
    }
    titleRow->addWidget(icon, 0, Qt::AlignTop);

    auto *title = new QLabel(shared->title().isEmpty() ? host : shared->title(), toast);
    title->setStyleSheet(QStringLiteral("color: %1; font-weight: 600; font-size: 12px;"
                                        "background: transparent;")
                             .arg(pal.text.name()));
    title->setWordWrap(true);
    title->setMaximumWidth(kToastWidth - 70);
    titleRow->addWidget(title, 1);

    auto *closeButton = new QToolButton(toast);
    closeButton->setObjectName(QStringLiteral("NotificationCloseButton"));
    closeButton->setIcon(Theme::icon(QStringLiteral("close"), 12));
    closeButton->setIconSize(QSize(12, 12));
    closeButton->setAutoRaise(true);
    closeButton->setFixedSize(20, 20);
    closeButton->setCursor(Qt::ArrowCursor);
    closeButton->setToolTip(get_text("notification_dismiss"));
    closeButton->setStyleSheet(
        QStringLiteral("QToolButton { border: none; background: transparent; }"
                       "QToolButton:hover { background: %1; border-radius: 4px; }")
            .arg(pal.hover.name()));
    titleRow->addWidget(closeButton, 0, Qt::AlignTop);
    layout->addLayout(titleRow);

    QString message = shared->message();
    if (message.size() > kMaxMessageChars)
        message = message.left(kMaxMessageChars) + QStringLiteral("\u2026");
    auto *body = new QLabel(message, toast);
    body->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; background: transparent;")
                            .arg(pal.dim.name()));
    body->setWordWrap(true);
    body->setMaximumWidth(kToastWidth - 24);
    layout->addWidget(body);

    auto *originLabel = new QLabel(host, toast);
    originLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; background: transparent;")
                                   .arg(pal.dim.name()));
    layout->addWidget(originLabel);

    // The toast is a click target; its labels must not swallow the click.
    for (QLabel *label : toast->findChildren<QLabel *>())
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // show() tells the page the notification is visible, which sites use to
    // decide whether they may update it later.
    shared->show();

    // Timeout and the dismiss button both go through dismiss(), which runs the
    // page's close() handler before the widget dies — see NotificationToast.
    auto *timer = new QTimer(toast);
    timer->setSingleShot(true);
    timer->setInterval(kLifetimeMs);
    connect(timer, &QTimer::timeout, toast, &NotificationToast::dismiss);
    timer->start();

    connect(closeButton, &QToolButton::clicked, toast, &NotificationToast::dismiss);
    connect(toast, &QObject::destroyed, this, [this, toast]() { removeToast(toast); });
    connect(toast, &NotificationToast::activated, this, [this, origin]() {
        if (m_activate && origin.isValid())
            m_activate(origin);
    });

    m_toasts.append(toast);
    // show() rather than setVisible(true): the toast paints itself and needs its
    // first layout pass before positionToasts() can size it.
    toast->show();
    positionToasts();
    emit shown(origin, shared->title());
}

void NotificationPresenter::removeToast(NotificationToast *toast)
{
    m_toasts.removeAll(toast);
    positionToasts();
}

void NotificationPresenter::evictOldest()
{
    if (m_toasts.size() < kMaxVisible)
        return;
    if (NotificationToast *oldest = m_toasts.takeFirst())
        oldest->dismiss();
}

void NotificationPresenter::positionToasts()
{
    if (!m_host)
        return;
    // Bottom-right of the window, the same corner Chrome uses, in window
    // coordinates so the toasts follow the window instead of being left behind
    // when it moves.
    const QRect area = m_host->rect();
    int y = area.bottom() - kMargin;
    for (NotificationToast *toast : m_toasts) {
        if (!toast)
            continue;
        const int h = qMax(kMinToastHeight, toast->sizeHint().height());
        if (toast->height() != h)
            toast->setFixedHeight(h);
        y -= h;
        toast->move(area.right() - kMargin - kToastWidth, y);
        toast->raise();
        y -= kSlotGap;
    }
}
