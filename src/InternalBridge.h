#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

class BookmarkManager;
class HistoryManager;
class DownloadManager;
class WebExtensionManager;
class LocalFileService;

// ---------------------------------------------------------------------------
// The `internalBridge` object behind cloudar://bookmarks, /history, /downloads,
// /extensions and /permissions.
//
// Those five pages were written against a QWebChannel object called
// `internalBridge` that nothing ever registered, so every one of them threw on
// `channel.objects.internalBridge.getBookmarks(...)` and rendered its empty
// state ("No bookmarks in this folder", "No history found", ...) no matter what
// the browser actually contained.
//
// --- How the reply reaches JavaScript -------------------------------------
// A QWebChannel method call is not a stream: qwebchannel.js strips the trailing
// function argument out of the call, keeps it as `callback`, sends only the
// remaining arguments, and then invokes `callback` with whatever the C++ slot
// *returns*:
//
//     bridge.getHistory(200, function (items) { ... });
//       -> C++: QVariant getHistory(int limit)
//       -> `items` is the returned value
//
// So these slots return data and never take a callback parameter. They return
// QVariantList/QVariantMap rather than a JSON string because that is what the
// pages already expect: they index and iterate the reply directly
// (`items.forEach(...)`, `Object.keys(data)`, `data.folders`). SettingsBridge
// returns a JSON string instead because settings.html explicitly JSON.parses it
// — the difference is deliberate, not an oversight.
// ---------------------------------------------------------------------------
class InternalBridge : public QObject
{
    Q_OBJECT
public:
    InternalBridge(BookmarkManager *bookmarks, HistoryManager *history,
                   DownloadManager *downloads, WebExtensionManager *webExtensions,
                   QObject *parent = nullptr);

public slots:
    // --- cloudar://bookmarks -------------------------------------------
    // { "folders": [...], "bookmarks": [{title,url,folder}] }
    QVariantMap getBookmarks();
    void addBookmark(const QString &title, const QString &url, const QString &folder);
    void removeBookmark(const QString &url);
    void addFolder(const QString &name);
    void removeFolder(const QString &name, bool withBookmarks);

    // --- cloudar://history ---------------------------------------------
    // [{title,url,visited}] newest first, capped at `limit`.
    QVariantList getHistory(int limit);
    QVariantList searchHistory(const QString &query);
    void clearHistory();

    // --- cloudar://downloads -------------------------------------------
    // [{filename,path,progress,completed,cancelled,paused}]
    QVariantList getDownloads();
    void openDownload(const QString &filename);
    void showInFolder(const QString &filename);
    void pauseDownload(const QString &filename);
    void resumeDownload(const QString &filename);
    void cancelDownload(const QString &filename);

    // --- cloudar://extensions ------------------------------------------
    // [{id,name,version,description,enabled}]
    QVariantList getExtensions();
    bool toggleExtension(const QString &id, bool enabled);

    // --- cloudar://permissions -----------------------------------------
    // { host: { capability: "allow" | "deny" } }
    QVariantMap getPermissions();
    void setPermission(const QString &host, const QString &capability, const QString &decision);
    void clearPermissions();

    // --- cloudar://files ------------------------------------------------
    // A directory listing, confined to a folder the user opened. Everything
    // that decides what may be listed lives in LocalFileService; these slots are
    // only the transport, so the page cannot reach a path the native side has
    // not already accepted.
    //
    // `directory` empty means the most recently opened root. The reply is
    // { ok, path, name, parent, entries, truncated, error } — `error` is a code
    // ("outside-root", "no-root", ...), never a sentence: the page translates it.
    QVariantMap listDirectory(const QString &directory);
    // Opens a folder as a root and returns its listing, so "Open Folder..." and
    // a click on a folder row in the page go through the same call.
    QVariantMap openDirectory(const QString &directory);
    // The roots the user has opened this session, most recent first.
    QStringList directoryRoots();
    // Asks the window for its own folder picker. A page cannot choose a folder
    // itself — that is what keeps the confinement meaningful — so this only
    // reports the request upward; MainWindow runs the dialog and the new root
    // arrives back through filesRootsChanged.
    void requestFolderPicker();

signals:
    // Emitted after anything a page displays changes, so an open page can
    // refresh itself instead of going stale while the user works elsewhere.
    void filesRootsChanged();
    // cloudar://files asked for a folder picker (see requestFolderPicker).
    void folderPickerRequested();
    void bookmarksChanged();
    void historyChanged();
    void downloadsChanged();
    void extensionsChanged();
    void permissionsChanged();

private:
    BookmarkManager *m_bookmarks = nullptr;
    HistoryManager *m_history = nullptr;
    DownloadManager *m_downloads = nullptr;
    WebExtensionManager *m_webExtensions = nullptr;
    // Owns the path-confinement rule for cloudar://files. Created here rather
    // than passed in because nothing else in the window needs it.
    LocalFileService *m_localFiles = nullptr;
};
