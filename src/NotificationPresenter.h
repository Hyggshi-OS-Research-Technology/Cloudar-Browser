#pragma once

#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QList>
#include <QString>

#include <functional>
#include <memory>

class QWidget;
class QWebEngineProfile;
class QWebEngineNotification;
class NotificationToast;

// ---------------------------------------------------------------------------
// Web notifications, for real.
//
// The roadmap line for this item read "notifications: declare unsupported",
// written on the assumption that QtWebEngine has no notification API. It does:
// QWebEngineProfile::setNotificationPresenter() has existed since Qt 5.13 and
// hands over a QWebEngineNotification carrying origin, title, message, icon and
// click()/close(). So this is implemented rather than disclaimed, with its
// limits written down where a user can read them (docs/KNOWN_ISSUES.md §12).
//
// What the user gets:
//   * a toast in the corner of the window that *sent* the notification, so a
//     site open in two windows does not pop up twice in one and never in the
//     other,
//   * click -> the page's own click() handler runs (that is what raises focus
//     on the site's side),
//   * dismiss or timeout -> the page's close() handler runs, so the site knows
//     the notification is gone instead of waiting forever,
//   * at most three at a time, oldest evicted,
//   * the same gate as Chrome: only a site the user allowed may show anything.
//     The normal permission prompt (MainWindow::requestPermission) is what
//     writes that decision, and a denied site is dropped silently — the page
//     must not be able to tell "denied" from "nobody is watching".
//
// What it is *not*:
//   * not a platform notification. There is no libnotify / Toast XML /
//     NSUserNotificationToast path in this build, so toasts only exist while the
//     Cloudar window exists. A closed window shows nothing, and nothing is
//     delivered later — there is no notification centre.
//   * no action buttons, no requireInteraction, no silent/renotify handling.
//   * the `tag` is not used to replace an existing toast; the engine's own
//     matches() already collapses notifications that the page means as updates.
// ---------------------------------------------------------------------------
class NotificationPresenter : public QObject
{
    Q_OBJECT
public:
    explicit NotificationPresenter(QObject *parent = nullptr);

    // Installs itself on the profile. Call once, after the profile exists.
    void installOn(QWebEngineProfile *profile);

    // Where toasts appear, and where they are positioned inside. Until this is
    // set, incoming notifications are dropped rather than queued forever: a
    // notification with no window to appear in has nowhere to go.
    void setHost(QWidget *host);

    // "The user activated this notification": MainWindow resolves the origin to
    // a tab and raises it. Kept as a callback because the presenter has no idea
    // what a tab is.
    void setActivateHandler(std::function<void(const QUrl &origin)> handler);

    static int maxVisible();
    // How many toast cards are on screen right now. Public because it is the
    // only thing an automated check can assert about a widget stack.
    int visibleCount() const { return static_cast<int>(m_toasts.size()); }

public slots:
    // Repaint every live toast after a light/dark flip. The card paints itself,
    // so it cannot pick the change up from the application stylesheet.
    void restyle();

signals:
    // Fired once per notification actually displayed, which is what the tests
    // and the status bar use. Not fired for a denied or dropped one.
    void shown(const QUrl &origin, const QString &title);

private:
    void handle(std::unique_ptr<QWebEngineNotification> notification);
    void evictOldest();
    void positionToasts();
    void removeToast(NotificationToast *toast);

    QPointer<QWidget> m_host;
    std::function<void(const QUrl &origin)> m_activate;
    QList<NotificationToast *> m_toasts; // oldest first
};
