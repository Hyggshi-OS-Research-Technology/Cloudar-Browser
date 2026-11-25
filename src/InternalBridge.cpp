#include "InternalBridge.h"

#include "BookmarkManager.h"
#include "DownloadManager.h"
#include "HistoryManager.h"
#include "LocalFileService.h"
#include "PermissionStore.h"
#include "WebExtensionManager.h"

#include <QDateTime>
#include <QVariantList>
#include <QVariantMap>

namespace {

QString isoTime(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString(Qt::ISODate) : QString();
}

} // namespace

InternalBridge::InternalBridge(BookmarkManager *bookmarks, HistoryManager *history,
                               DownloadManager *downloads, WebExtensionManager *webExtensions,
                               QObject *parent)
    : QObject(parent), m_bookmarks(bookmarks), m_history(history),
      m_downloads(downloads), m_webExtensions(webExtensions),
      m_localFiles(new LocalFileService(this))
{
    if (m_bookmarks)
        connect(m_bookmarks, &BookmarkManager::changed, this, &InternalBridge::bookmarksChanged);
    if (m_history)
        connect(m_history, &HistoryManager::changed, this, &InternalBridge::historyChanged);
    if (m_webExtensions)
        connect(m_webExtensions, &WebExtensionManager::extensionsChanged,
                this, &InternalBridge::extensionsChanged);
    if (m_downloads)
        connect(m_downloads, &DownloadManager::changed, this, &InternalBridge::downloadsChanged);
    if (m_localFiles)
        connect(m_localFiles, &LocalFileService::rootsChanged,
                this, &InternalBridge::filesRootsChanged);
}

// ---------------------------------------------------------------------------
// Bookmarks
// ---------------------------------------------------------------------------

QVariantMap InternalBridge::getBookmarks()
{
    QVariantList bookmarks;
    QVariantList folders;

    if (m_bookmarks) {
        for (const BookmarkEntry &entry : m_bookmarks->entries()) {
            QVariantMap obj;
            obj["title"] = entry.title;
            obj["url"] = entry.url;
            obj["folder"] = entry.folder;
            bookmarks.append(obj);
        }
        for (const QString &folder : m_bookmarks->folders())
            folders.append(folder);
    }

    QVariantMap payload;
    payload["folders"] = folders;
    payload["bookmarks"] = bookmarks;
    return payload;
}

void InternalBridge::addBookmark(const QString &title, const QString &url, const QString &folder)
{
    if (!m_bookmarks)
        return;
    // The page prompts for the URL as free text, so a bare "example.com" has to
    // become a real URL before it is stored — otherwise the entry is truncated
    // to a relative path by every consumer that resolves it later.
    const QUrl parsed = QUrl::fromUserInput(url.trimmed());
    if (!parsed.isValid() || parsed.isEmpty())
        return;
    m_bookmarks->add(title.trimmed(), parsed.toString(), folder.trimmed());
}

void InternalBridge::removeBookmark(const QString &url)
{
    if (!m_bookmarks)
        return;
    const int index = m_bookmarks->indexOfUrl(url);
    if (index >= 0)
        m_bookmarks->removeAt(index);
}

void InternalBridge::addFolder(const QString &name)
{
    if (m_bookmarks)
        m_bookmarks->addFolder(name);
}

void InternalBridge::removeFolder(const QString &name, bool withBookmarks)
{
    if (m_bookmarks)
        m_bookmarks->removeFolder(name, withBookmarks);
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------

QVariantList InternalBridge::getHistory(int limit)
{
    QVariantList list;
    if (!m_history)
        return list;

    const QVector<HistoryEntry> entries = m_history->entries();
    // Already newest-first: HistoryManager::addVisit() prepends, and load()
    // restores the file in that same order. No reversal here — reversing would
    // show the oldest visits at the top of the page.
    const int cap = limit > 0 ? qMin(limit, entries.size()) : entries.size();
    for (int i = 0; i < cap; ++i) {
        const HistoryEntry &entry = entries[i];
        QVariantMap obj;
        obj["title"] = entry.title;
        obj["url"] = entry.url;
        obj["visited"] = isoTime(entry.visitedAt);
        list.append(obj);
    }
    return list;
}

QVariantList InternalBridge::searchHistory(const QString &query)
{
    QVariantList list;
    const QString needle = query.trimmed();
    if (!m_history || needle.isEmpty())
        return list;

    // Same newest-first order as getHistory(); walk forward, not backward.
    const QVector<HistoryEntry> entries = m_history->entries();
    for (const HistoryEntry &entry : entries) {
        if (!entry.title.contains(needle, Qt::CaseInsensitive)
            && !entry.url.contains(needle, Qt::CaseInsensitive))
            continue;
        QVariantMap obj;
        obj["title"] = entry.title;
        obj["url"] = entry.url;
        obj["visited"] = isoTime(entry.visitedAt);
        list.append(obj);
    }
    return list;
}

void InternalBridge::clearHistory()
{
    if (!m_history)
        return;
    m_history->clear();
    emit historyChanged();
}

// ---------------------------------------------------------------------------
// Downloads
// ---------------------------------------------------------------------------

QVariantList InternalBridge::getDownloads()
{
    QVariantList list;
    if (!m_downloads)
        return list;

    for (const DownloadRecord &record : m_downloads->records()) {
        QVariantMap obj;
        obj["filename"] = record.filename;
        obj["path"] = record.path;
        obj["progress"] = record.progress;
        obj["completed"] = record.completed;
        obj["cancelled"] = record.cancelled;
        obj["paused"] = record.paused;
        list.append(obj);
    }
    return list;
}

void InternalBridge::openDownload(const QString &filename)
{
    if (m_downloads)
        m_downloads->openDownload(filename);
}

void InternalBridge::showInFolder(const QString &filename)
{
    if (m_downloads)
        m_downloads->showInFolder(filename);
}

void InternalBridge::pauseDownload(const QString &filename)
{
    if (m_downloads)
        m_downloads->pauseDownload(filename);
}

void InternalBridge::resumeDownload(const QString &filename)
{
    if (m_downloads)
        m_downloads->resumeDownload(filename);
}

void InternalBridge::cancelDownload(const QString &filename)
{
    if (m_downloads)
        m_downloads->cancelDownload(filename);
}

// ---------------------------------------------------------------------------
// Extensions
// ---------------------------------------------------------------------------

QVariantList InternalBridge::getExtensions()
{
    QVariantList list;
    if (!m_webExtensions)
        return list;

    for (const LoadedWebExtension &ext : m_webExtensions->extensions()) {
        QVariantMap obj;
        obj["id"] = ext.id;
        obj["name"] = ext.manifest.name;
        obj["version"] = ext.manifest.version;
        obj["description"] = ext.manifest.description;
        obj["enabled"] = ext.enabled;
        list.append(obj);
    }
    return list;
}

bool InternalBridge::toggleExtension(const QString &id, bool enabled)
{
    if (!m_webExtensions)
        return false;
    m_webExtensions->setEnabled(id, enabled);
    // The page passes an empty callback and re-renders from extensionsChanged()
    // instead: setEnabled() rescans the disk, which can add or remove rows, so a
    // simple "ok" would not describe what actually happened.
    return true;
}

// ---------------------------------------------------------------------------
// Permissions
// ---------------------------------------------------------------------------

QVariantMap InternalBridge::getPermissions()
{
    return PermissionStore::allDecisions();
}

void InternalBridge::setPermission(const QString &host, const QString &capability,
                                   const QString &decision)
{
    PermissionStore::setDecision(host, capability, decision);
    emit permissionsChanged();
}

void InternalBridge::clearPermissions()
{
    PermissionStore::clearAll();
    emit permissionsChanged();
}

// ---------------------------------------------------------------------------
// Local files
// ---------------------------------------------------------------------------

QVariantMap InternalBridge::listDirectory(const QString &directory)
{
    if (!m_localFiles) {
        QVariantMap payload;
        payload[QStringLiteral("ok")] = false;
        payload[QStringLiteral("error")] = QStringLiteral("no-service");
        return payload;
    }
    // LocalFileService re-checks the confinement itself; doing it only here
    // would leave the rule in a transport layer, where the next caller to reach
    // the service directly would bypass it.
    return m_localFiles->listDirectory(directory);
}

QVariantMap InternalBridge::openDirectory(const QString &directory)
{
    if (!m_localFiles) {
        QVariantMap payload;
        payload[QStringLiteral("ok")] = false;
        payload[QStringLiteral("error")] = QStringLiteral("no-service");
        return payload;
    }
    m_localFiles->addRoot(directory);
    return m_localFiles->listDirectory(directory);
}

QStringList InternalBridge::directoryRoots()
{
    return m_localFiles ? m_localFiles->roots() : QStringList();
}

void InternalBridge::requestFolderPicker()
{
    emit folderPickerRequested();
}
