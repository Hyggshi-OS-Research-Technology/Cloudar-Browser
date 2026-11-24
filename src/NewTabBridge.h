#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class BookmarkManager;
class HistoryManager;
class FaviconStore;

// ---------------------------------------------------------------------------
// `newTabBridge` — the internal API behind cloudar://newtab.
//
// The New Tab page used to be a single 1100-line file that derived everything it
// showed from localStorage and from letters standing in for icons. Two things
// were wrong with that, and both are the reason this class exists:
//
//   1. the page re-implemented state the browser already had (bookmarks,
//      search engine, interface style, favicons), so it could drift, and did —
//      the classic chrome had no rules on that page so the class it toggled did
//      nothing at all;
//   2. it had no way to ask for anything real, so the speed dial invented an
//      avatar from the first letter of the site name.
//
// Everything below is read-only except the navigation actions: adding and
// removing shortcuts stays in the page (they are page state, not browser
// state), but what a shortcut *is* comes from here.
//
// Naming note: this object is deliberately separate from `native` and
// `settingsBridge`. It is registered on the same QWebChannel, which is attached
// only to cloudar:// pages (see BrowserTabWidget::wireView), so a public website
// can never reach it.
// ---------------------------------------------------------------------------
class NewTabBridge : public QObject
{
    Q_OBJECT
public:
    NewTabBridge(BookmarkManager *bookmarks, HistoryManager *history, FaviconStore *favicons,
                 QObject *parent = nullptr);

public slots:
    // One call for everything the page needs to render itself, so the New Tab
    // page does not make five round trips before it can show a pixel.
    //   { chromeStyle: "classic"|"modern",
    //     searchTemplate: "https://duckduckgo.com/?q=%s",
    //     searchName: "DuckDuckGo",
    //     version: "0.2.0",
    //     bookmarks: [{title,url}],           // for one-click shortcut addition
    //     recent:    [{title,url,visited}] }  // 6 newest, for "continue browsing"
    QVariantMap initialState();

    // "cloudar://favicon/?u=<encoded page url>", or "" when nothing is cached
    // yet. A hit is instant; a miss returns "" and schedules a background fetch,
    // after which faviconReady() fires instead of making the page wait.
    QString faviconUrl(const QString &pageUrl);

    // Navigation goes through the browser so a shortcut to cloudar://settings
    // reuses the open settings tab instead of stacking a second one.
    void openUrl(const QString &url);

signals:
    // (pageUrl, iconUrl) — iconUrl is exactly what faviconUrl() would return now
    // that the icon has been stored. The page matches on pageUrl, not on host,
    // so two tiles pointing at one site both update.
    void faviconReady(const QString &pageUrl, const QString &iconUrl);
    // MainWindow navigates the current tab. Emitted instead of letting the page
    // set location.href, so a shortcut to cloudar://settings focuses the tab
    // that is already open on it rather than opening a second copy.
    void navigationRequested(const QUrl &url);

private:
    BookmarkManager *m_bookmarks = nullptr;
    HistoryManager *m_history = nullptr;
    FaviconStore *m_favicons = nullptr;
};
