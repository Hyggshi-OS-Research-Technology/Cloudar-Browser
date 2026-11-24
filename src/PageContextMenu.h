#pragma once

#include <QAction>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QUrl>

#include <functional>

class QWebEngineView;
class QWebEngineContextMenuRequest;
class QWebEnginePage;

// ---------------------------------------------------------------------------
// Right-click menu for page content.
//
// The browser had no context menu on the page itself at all: right-clicking
// showed whatever QtWebEngine's platform integration provided, which on Linux
// is nothing and on Windows is a bare "Back / Forward / Reload / Save page /
// View page source" list with no icons, no copy/paste, no "open link in new tab"
// and no state (Back enabled on a page with no history). That list is the
// screenshot this class was written from.
//
// The menu is built from QWebEngineContextMenuRequest, so it reflects what is
// actually under the cursor: link, image, selected text, editable field — and
// Back/Forward are enabled only when there is history to go to.
//
// Chrome-parity is the goal, honesty is the constraint: an item that cannot do
// anything here (e.g. "Save image as" on a blob: URL served by a custom scheme)
// is either wired to something real or omitted, never left in as a no-op.
// ---------------------------------------------------------------------------
class PageContextMenu : public QObject
{
    Q_OBJECT
public:
    explicit PageContextMenu(QWebEngineView *view, QObject *parent = nullptr);

    // Builds and shows the menu at `pos` (view coordinates): the slot for
    // QWebEngineView::customContextMenuRequested.
    void showAt(const QPoint &pos);

    // Lets MainWindow offer "Bookmark this page" only when it can act on it.
    // A provider rather than a pointer because the action is created by the
    // toolbar, which may not exist yet when the tabs are built.
    void setBookmarkActionProvider(std::function<QAction *()> provider)
    {
        m_bookmarkActionProvider = std::move(provider);
    }
    void setPageSourceHandler(std::function<void(const QUrl &)> handler)
    {
        m_pageSourceHandler = std::move(handler);
    }

signals:
    // Handled by MainWindow, which owns tab creation and the search box, so this
    // class stays a menu and nothing else.
    void openInNewTab(const QUrl &url);
    void openInNewWindow(const QUrl &url);
    void searchRequested(const QString &text);
    void printRequested();
    // MainWindow owns the DevTools window; the menu only asks for it.
    void devToolsRequested();

private:
    QWebEngineView *m_view = nullptr;
    std::function<QAction *()> m_bookmarkActionProvider;
    std::function<void(const QUrl &)> m_pageSourceHandler;
};
