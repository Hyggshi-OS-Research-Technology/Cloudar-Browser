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

    // Wallpaper bytes handed to the browser instead of being kept in the
    // renderer's storage.
    //
    // Why this exists at all: a cloudar:// page's localStorage and IndexedDB
    // belong to the renderer's *storage partition*, and this build clears
    // browsing data on exit for anyone who did not opt in to persistent cookies
    // — "open it again and the wallpaper is gone" was exactly that. QSettings is
    // tied to the installation, not to the browsing session, so a wallpaper
    // saved here survives, and it also survives clearing browsing data, which is
    // what a person expects of a preference they chose deliberately.
    //
    // `base64` is the raw file bytes, already re-encoded by the page. `kind` is
    // "image" | "video" | "web" | "color" | "default"; `value` carries the
    // URL/colour for the last three.
    //
    // `mime` ("image/png", "image/webp", "image/jpeg", ...) describes those
    // bytes. It decides the stored file's extension, and therefore the
    // Content-Type the scheme handler answers with, so the extension is never a
    // guess. An empty or unrecognised value falls back to JPEG for images and
    // MP4 for videos, which is what the page used to send unconditionally.
    //
    // Returns the cloudar://newtab/media/<file> URL to display, or "" on failure.
    QString saveWallpaperMedia(const QString &kind, const QString &base64,
                               const QString &value, const QString &mime);

    // The cloudar:// URL for the stored wallpaper, or "" when none is stored.
    // The page asks for this at startup (initialState().wallpaper comes from the
    // same code) so it never has to guess whether its media survived.
    QString wallpaperMediaUrl();

    // What is currently stored: { kind, value, url, bytes, framing } — kind is
    // "default" when nothing is set. `bytes` is the real file size, so the panel
    // can say how much disk the wallpaper uses instead of estimating. `framing`
    // is { x, y, zoom } and is only meaningful for an image or video wallpaper.
    QVariantMap wallpaperState();

    void clearWallpaperMedia();

    // How the wallpaper sits in the window: the point of the image that is
    // centred (focusX/focusY, each -1..1) and how far in it is zoomed (zoom,
    // 1..4). At 0,0,1 the wallpaper is the centred cover it always was.
    //
    // This is a deliberate choice like the wallpaper itself, so it lives in
    // QSettings with it and survives a restart — the page's localStorage is in
    // the storage partition and gets cleared. Values are clamped here, not
    // trusted from the page: a zoom of 0 or a focus of 40 would otherwise
    // render a blank window with no way back through the UI.
    //
    // Returns true when the value was stored.
    bool saveWallpaperFraming(double focusX, double focusY, double zoom);

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
