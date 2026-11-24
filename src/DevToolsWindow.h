#pragma once

#include <QHash>
#include <QPointer>
#include <QWidget>

class QWebEnginePage;
class QWebEngineView;

// ---------------------------------------------------------------------------
// A real DevTools window.
//
// QtWebEngineWidgets does provide the inspector, but only as a bare
// QWebEngineView that Qt creates for itself: it has no window title, no way to
// reopen the same one, and — the part that made it useless here — nothing tells
// the user how to reach it. `QWebEnginePage::triggerAction(InspectElement)` and
// F12 were both unwired, so there was no way in at all from the browser.
//
// DevTools here is one window per inspected page:
//
//     frontend->setInspectedPage(target);          // devtools://…/inspector.html
//     frontend->setUrl(QUrl("devtools://devtools/bundled/inspector.html"));
//
// The frontend page is created with the *inspected* page's profile so that it
// shares the same storage partition — Qt requires this, and a mismatch shows up
// as a DevTools window that connects to nothing.
//
// Closing the window only hides it: reopening DevTools for the same page brings
// back the same console, which is what every other browser does.
// ---------------------------------------------------------------------------
class DevToolsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit DevToolsWindow(QWebEnginePage *inspected, QWidget *parent = nullptr);

    // The page this window is attached to. Used to find an existing window.
    QWebEnginePage *inspectedPage() const { return m_inspected; }

    // Re-shows an existing window for `page`, or builds one. Returns nullptr if
    // the browser cannot host DevTools for that page (no profile, null page).
    static DevToolsWindow *showFor(QWebEnginePage *page, QWidget *parent);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QPointer<QWebEnginePage> m_inspected;
    QWebEngineView *m_view = nullptr;
    QWebEnginePage *m_frontendPage = nullptr;

    static QHash<QWebEnginePage *, DevToolsWindow *> s_windows;
};
