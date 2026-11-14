#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <functional>

class QTimer;

// The complete C++ half of the chrome.* API surface: permissions, storage,
// messaging, tabs, scripting, alarms, and the extension action (toolbar
// button). One instance is shared by every extension context that exists —
// every background page and every content script injected into an ordinary
// tab — all talking to it over one QWebChannel object named "chromeBridge"
// (see WebExtensionManager). Every call carries an explicit extensionId so
// contexts belonging to different extensions can never read each other's
// storage, messages, or state, even though they share one bridge object.
//
// This supersedes the old WebExtensionAPIBridge: same "load unpacked"
// developer-mode scope (still no sandboxed multi-process Chromium host —
// see WebExtensionManager's header for exactly what QtWebEngine can and
// can't isolate), but now covers a wider chrome.* surface:
//   - storage.local (persisted, QSettings-backed) + storage.session
//     (in-memory, cleared on process exit or extension reload — matches
//     real Chrome's session-storage lifetime)
//   - runtime messaging (fire-and-listen broadcast, not full ports)
//   - permissions.contains, now covering both API permissions and URL
//     match-pattern "origins"
//   - tabs.query/create/update/remove
//   - scripting.executeScript (requires the "scripting" permission *and*
//     either "activeTab" or a matching host_permissions entry for the
//     target tab's current URL — checked here, not just hidden in JS)
//   - alarms.create/clear/clearAll/getAll + onAlarm
//   - action.setBadgeText/setBadgeBackgroundColor/setTitle (backend only;
//     wiring these into an actual toolbar button is left to MainWindow /
//     ExtensionsDialog — see badgeChanged/titleChanged/actionClicked)
//
// Every capability that touches something outside the extension's own
// storage (tabs, scripting, alarms) re-checks hasPermission()/
// hasHostPermission() itself rather than trusting the JS shim's own
// gating, since the shim only *hides* APIs an extension didn't declare —
// it doesn't stop a modified/malicious content script from calling the
// bridge slot directly.
class ChromeExtensionAPI : public QObject
{
    Q_OBJECT
public:
    explicit ChromeExtensionAPI(QObject *parent = nullptr);

    // --- wiring, done once by WebExtensionManager after construction ---
    void setPermissionLookup(std::function<QStringList(const QString &extensionId)> fn);
    void setHostPermissionLookup(std::function<QStringList(const QString &extensionId)> fn);
    void setTabsProvider(std::function<QString()> fn); // JSON array of open tabs, for tabs.query

    // tabId in every one of these is the tab's index within BrowserTabWidget
    // at call time — the same "id" convention chrome.tabs.query's JSON
    // already uses (see MainWindow's tabsProvider lambda). It's not a
    // stable handle across tabs closing/reordering, same caveat as before.
    void setTabController(std::function<int(const QString &url, bool active)> createFn,
                           std::function<bool(int tabId, const QString &url)> updateFn,
                           std::function<bool(int tabId)> removeFn);
    void setTabUrlLookup(std::function<QString(int tabId)> fn);
    void setScriptRunner(std::function<bool(int tabId, const QString &code,
                                             std::function<void(const QString &resultJson)> callback)> fn);

    // Called by WebExtensionManager when an extension is unloaded/reloaded:
    // drops its alarms and session storage (both process-lifetime state),
    // but deliberately leaves storage.local alone — that one's meant to
    // survive reloads, matching real Chrome.
    void clearExtensionState(const QString &extensionId);

public slots:
    // --- chrome.storage.local / chrome.storage.session ---
    // `area` is "local" or "session". `keysJson` is "null" (get everything)
    // or a JSON array of key names. Returns a JSON object of {key: value}.
    QString storageGet(const QString &extensionId, const QString &area, const QString &keysJson) const;
    void storageSet(const QString &extensionId, const QString &area, const QString &itemsJson);
    void storageRemove(const QString &extensionId, const QString &area, const QString &keysJson);
    void storageClear(const QString &extensionId, const QString &area);

    // --- chrome.runtime messaging ---
    // Broadcasts `messageJson` to every other loaded context of the same
    // extension. Simple fire-and-listen, not the full per-message
    // response/port model real Chrome has.
    void runtimeSendMessage(const QString &extensionId, const QString &senderContextId,
                             const QString &messageJson);

    // --- chrome.permissions ---
    bool hasPermission(const QString &extensionId, const QString &permission) const;
    bool hasHostPermission(const QString &extensionId, const QString &url) const;

    // --- chrome.tabs ---
    QString tabsQuery(const QString &extensionId) const; // ignores query filters, returns all tabs
    int tabsCreate(const QString &extensionId, const QString &url, bool active);
    bool tabsUpdate(const QString &extensionId, int tabId, const QString &url);
    bool tabsRemove(const QString &extensionId, int tabId);

    // --- chrome.scripting.executeScript ---
    // Async: result comes back via the scriptExecuted signal, matched to
    // the caller by requestId (generated JS-side, opaque to us).
    void scriptingExecute(const QString &extensionId, const QString &contextId,
                           int tabId, const QString &code, const QString &requestId);

    // --- chrome.alarms ---
    void alarmsCreate(const QString &extensionId, const QString &name,
                       double delayInMinutes, double periodInMinutes);
    void alarmsClear(const QString &extensionId, const QString &name);
    void alarmsClearAll(const QString &extensionId);
    QString alarmsGetAll(const QString &extensionId) const; // JSON array of {name, periodInMinutes}

    // --- chrome.action ---
    void actionSetBadgeText(const QString &extensionId, const QString &text);
    void actionSetBadgeBackgroundColor(const QString &extensionId, const QString &color);
    void actionSetTitle(const QString &extensionId, const QString &title);

    // Not emitted by this class on its own — call this from wherever the
    // browser actually renders a per-extension toolbar button once one
    // exists, so chrome.action.onClicked listeners fire correctly.
    void notifyActionClicked(const QString &extensionId, int tabId);

signals:
    void messageReceived(const QString &extensionId, const QString &senderContextId,
                          const QString &messageJson);
    void scriptExecuted(const QString &extensionId, const QString &contextId,
                         const QString &requestId, const QString &resultJson);
    void alarmFired(const QString &extensionId, const QString &name);
    void badgeChanged(const QString &extensionId, const QString &text, const QString &backgroundColor);
    void titleChanged(const QString &extensionId, const QString &title);
    void actionClicked(const QString &extensionId, int tabId);

private:
    struct AlarmEntry {
        QTimer *timer = nullptr;
        double periodInMinutes = 0.0;
    };

    QString storageGroup(const QString &extensionId, const QString &area) const;
    static bool urlMatchesPattern(const QString &pattern, const QString &url);

    std::function<QStringList(const QString &)> m_permissionLookup;
    std::function<QStringList(const QString &)> m_hostPermissionLookup;
    std::function<QString()> m_tabsProvider;
    std::function<int(const QString &, bool)> m_tabCreate;
    std::function<bool(int, const QString &)> m_tabUpdate;
    std::function<bool(int)> m_tabRemove;
    std::function<QString(int)> m_tabUrlLookup;
    std::function<bool(int, const QString &, std::function<void(const QString &)>)> m_scriptRunner;

    // storage.session: in-memory only, extensionId -> {key: jsonEnvelope}
    QHash<QString, QHash<QString, QString>> m_sessionStorage;
    // alarms: extensionId -> alarmName -> entry
    QHash<QString, QHash<QString, AlarmEntry>> m_alarms;
    // action state, kept here so a future toolbar button can query current
    // state instead of only reacting to the changed signals
    QHash<QString, QString> m_badgeText;
    QHash<QString, QString> m_badgeColor;
};
