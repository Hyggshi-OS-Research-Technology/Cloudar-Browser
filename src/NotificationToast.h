#pragma once

#include <QFrame>

#include <memory>

class QWebEngineNotification;

// ---------------------------------------------------------------------------
// One notification card.
//
// A QFrame subclass rather than a styled QFrame because it paints its own
// rounded surface: the card floats over web content, so it cannot inherit the
// window's background, and a stylesheet on a translucent widget gives Qt square
// corners no matter what border-radius it was given.
//
// Public (not file-local) because it needs Q_OBJECT for its `activated` signal,
// and Qt's AUTOMOC only generates code for classes declared in headers.
//
// The card does not own its lifetime policy: NotificationPresenter sets
// WA_DeleteOnClose and connects to destroyed(). It *does* own the
// QWebEngineNotification, deliberately — destroying the card is what runs the
// page's close() handler, so a toast that is dismissed, times out or is evicted
// always tells the site it is gone.
// ---------------------------------------------------------------------------
class NotificationToast : public QFrame
{
    Q_OBJECT
public:
    explicit NotificationToast(QWidget *parent = nullptr);

    void setNotification(std::shared_ptr<QWebEngineNotification> notification);
    QWebEngineNotification *notification() const;

    // Closes the card *and* the page's notification, in that order of intent:
    // the page's close() handler is what it uses to stop waiting for a click.
    // Idempotent, because timeout, dismiss and eviction can all race.
    void dismiss();

signals:
    // The user clicked the card (not the dismiss button).
    void activated();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    std::shared_ptr<QWebEngineNotification> m_notification;
    bool m_pressed = false;
    bool m_dismissed = false;
};
