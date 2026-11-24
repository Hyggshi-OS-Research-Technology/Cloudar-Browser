#include "DevToolsWindow.h"

#include "ClassicTheme.h"

#include <QCloseEvent>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

QHash<QWebEnginePage *, DevToolsWindow *> DevToolsWindow::s_windows;

// The only frontend URL QtWebEngine 6 accepts. Anything else is rejected by
// DevToolsFrontendQt::IsValidFrontendURL(), and the window comes up blank.
static const char *kDevToolsUrl = "devtools://devtools/bundled/inspector.html";

DevToolsWindow::DevToolsWindow(QWebEnginePage *inspected, QWidget *parent)
    : QWidget(parent, Qt::Window), m_inspected(inspected)
{
    setWindowTitle(tr("Developer Tools"));
    resize(980, 680);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_view = new QWebEngineView(this);

    // The frontend page must live in the same profile as the inspected page;
    // DevTools connects over a shared storage partition. Fall back to the
    // default profile only if the page somehow has none.
    QWebEngineProfile *profile = inspected ? inspected->profile() : nullptr;
    m_frontendPage = profile ? new QWebEnginePage(profile, m_view)
                             : new QWebEnginePage(m_view);
    m_view->setPage(m_frontendPage);
    m_frontendPage->setInspectedPage(inspected);
    m_frontendPage->setUrl(QUrl(QString::fromLatin1(kDevToolsUrl)));

    layout->addWidget(m_view);

    // Two things give the window a usable identity: a title that says which page
    // is being inspected, and the page's own icon when it has one.
    if (inspected) {
        const auto retitle = [this]() {
            if (!m_inspected)
                return;
            const QString title = m_inspected->title();
            setWindowTitle(title.isEmpty() ? tr("Developer Tools")
                                           : tr("Developer Tools — %1").arg(title));
        };
        connect(inspected, &QWebEnginePage::titleChanged, this, [retitle](const QString &) { retitle(); });
        retitle();
    }
}

DevToolsWindow *DevToolsWindow::showFor(QWebEnginePage *page, QWidget *parent)
{
    if (!page)
        return nullptr;

    // One window per page: reopening brings back the same console and keeps the
    // scroll position, instead of stacking windows.
    if (DevToolsWindow *existing = s_windows.value(page, nullptr)) {
        existing->showNormal();
        existing->raise();
        existing->activateWindow();
        return existing;
    }

    auto *window = new DevToolsWindow(page, parent);
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    s_windows.insert(page, window);

    // Forget the entry when either side goes away, so the hash cannot keep a
    // dangling pointer to a closed tab's page.
    connect(window, &QObject::destroyed, window, [page]() {
        s_windows.remove(page);
    });
    connect(page, &QObject::destroyed, window, [page]() {
        if (DevToolsWindow *w = s_windows.take(page))
            w->close();
    });

    window->show();
    return window;
}

void DevToolsWindow::closeEvent(QCloseEvent *event)
{
    // Closing the window detaches the inspector so the inspected page stops
    // paying for the DevTools agent; the window itself is deleted by
    // WA_DeleteOnClose and the next open builds a fresh console.
    if (m_frontendPage)
        m_frontendPage->setInspectedPage(nullptr);
    QWidget::closeEvent(event);
}
