#pragma once

#include <QObject>
#include <QString>
#include <functional>

// The C++ half of the chrome.* API surface. A single instance is shared by
// every extension context that exists — every background page and every
// content script injected into an ordinary tab — all talking to it over one
// QWebChannel. Every call is scoped by an explicit `extensionId` argument
// (the JS side always supplies its own id) so extensions can't read each
// other's storage or messages, even though they share one bridge object.
//
// This deliberately covers a *subset* of chrome.*: storage.local,
// runtime messaging, permissions.contains, and a minimal tabs.query. There
// is no webRequest, declarativeNetRequest, scripting.executeScript,
// devtools, or Chrome Web Store install flow — this is "load unpacked"
// developer-mode extensions only, matching WebExtensionManager's scope.
class WebExtensionAPIBridge : public QObject
{
    Q_OBJECT
public:
    explicit WebExtensionAPIBridge(QObject *parent = nullptr);

    // Wired up by WebExtensionManager after construction so permission
    // checks and tab queries can reach real state without this class having
    // to know about WebExtensionManager or BrowserTabWidget directly.
    void setPermissionLookup(std::function<QStringList(const QString &extensionId)> fn);
    void setTabsProvider(std::function<QString()> fn); // returns a JSON array of open tabs

public slots:
    // --- chrome.storage.local --- (namespaced per extension under QSettings)
    // `keysJson` is either "null" (get everything) or a JSON array of key
    // names. Returns a JSON object of { key: value }.
    QString storageGet(const QString &extensionId, const QString &keysJson) const;
    // `itemsJson` is a JSON object of { key: value } to merge into storage.
    void storageSet(const QString &extensionId, const QString &itemsJson);
    void storageRemove(const QString &extensionId, const QString &keysJson);
    void storageClear(const QString &extensionId);

    // --- chrome.runtime messaging ---
    // Relays `messageJson` to every other loaded context of the same
    // extension (background page + all content-script instances). The
    // sender's own contextId is excluded so it never receives its own
    // message. This is a simple broadcast, not the real Chrome model of
    // per-message response callbacks/ports, but covers the common
    // fire-and-listen pattern extensions actually use most.
    void runtimeSendMessage(const QString &extensionId, const QString &senderContextId,
                             const QString &messageJson);

    // --- chrome.permissions ---
    bool hasPermission(const QString &extensionId, const QString &permission) const;

    // --- chrome.tabs (minimal) ---
    QString tabsQuery(const QString &extensionId) const; // ignores query filters, returns all tabs

signals:
    // Emitted for every runtimeSendMessage call; each context's JS shim
    // filters on extensionId and ignores messages whose senderContextId
    // matches its own.
    void messageReceived(const QString &extensionId, const QString &senderContextId,
                          const QString &messageJson);

private:
    QString storageGroup(const QString &extensionId) const;

    std::function<QStringList(const QString &)> m_permissionLookup;
    std::function<QString()> m_tabsProvider;
};
