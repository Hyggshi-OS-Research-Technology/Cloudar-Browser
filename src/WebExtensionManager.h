#pragma once

#include <QObject>
#include <QVector>
#include <QPointer>
#include <functional>

#include "WebExtensionManifest.h"

class QWebEngineProfile;
class QWebEnginePage;
class QWebChannel;
class ChromeExtensionAPI;

// One loaded, manifest.json-based extension. Distinct from
// ExtensionManager's simple Greasemonkey-style userscripts — this is a real
// extension folder ("Load unpacked" style) with its own permissions,
// content scripts, and background context.
struct LoadedWebExtension {
    QString id;                            // stable id derived from the folder name
    QString rootDir;                       // absolute path to the extension's folder
    WebExtensionManifest manifest;
    bool enabled = true;
    QPointer<QWebEnginePage> backgroundPage; // hidden page hosting the service worker, if any
};

// Loads and runs real, unpacked Chrome-style extensions (a Manifest V3
// subset) — see WebExtensionManifest.h for exactly which manifest fields
// are understood.
//
// Every extension gets:
//   - permissions read straight from its manifest.json
//   - content scripts injected into a JS world shared by all extensions
//     but isolated from ordinary page JS (EXTENSIONS_WORLD_ID), gated at
//     runtime against the manifest's `matches` patterns
//   - a hidden, headless QWebEnginePage running its background
//     service_worker, if it declares one
//   - a chrome.storage.local namespaced by extension id, and
//     chrome.runtime messaging scoped to that extension's own contexts
//
// What this is NOT: a sandboxed, multi-process Chromium extension host.
// QtWebEngine gives us one shared profile and one JS "isolated world" per
// id — good enough for content-script isolation from page JS, not for
// isolating extensions from a compromised renderer. There's no
// webRequest/declarativeNetRequest (can't intercept network requests this
// way), no scripting.executeScript, no options/devtools pages, and no
// Chrome Web Store install flow. This is meant for the same audience as
// ExtensionManager: people loading their own small tools, unpacked, not a
// general-purpose extension store.
class WebExtensionManager : public QObject
{
    Q_OBJECT
public:
    // Shared by every content script of every extension. World 0 is page
    // JS, world 1 is reserved by Qt (ApplicationWorld) — this just needs to
    // be >= 2 and not collide with anything else the app uses.
    static constexpr quint32 EXTENSIONS_WORLD_ID = 2;

    explicit WebExtensionManager(QWebEngineProfile *profile, QObject *parent = nullptr);

    QString extensionsRootDir() const; // ~/.local/share/lightbrowser/webextensions/
    QVector<LoadedWebExtension> extensions() const { return m_extensions; }

    // Rescans extensionsRootDir(): every immediate subfolder containing a
    // manifest.json is (re)loaded. Extensions removed from disk are
    // unloaded; existing background pages are torn down and recreated so
    // edited source is picked up.
    void reload();

    void setEnabled(const QString &extensionId, bool enabled);
    bool isEnabled(const QString &extensionId) const;

    // Call once for every QWebEnginePage the browser creates for a regular
    // (non-internal) tab, right after creation — see BrowserTabWidget's
    // wireView(). Attaches the shared extensions QWebChannel to the page's
    // isolated world so enabled extensions' content scripts can reach
    // chrome.*. Safe to call unconditionally; pages that never load a
    // matching content script simply never use it.
    void attachToPage(QWebEnginePage *page);

    // Wired from MainWindow post-construction so chrome.tabs.query has real
    // data to return. `provider` should return a JSON array of
    // {id,url,title,active} objects for every open tab.
    void setTabsProvider(std::function<QString()> provider);

    // Wired from MainWindow so chrome.tabs.create/update/remove and
    // chrome.scripting.executeScript can act on real tabs instead of being
    // permanently unimplemented — see ChromeExtensionAPI.h for exactly
    // what's checked before any of these actually run.
    void setTabController(std::function<int(const QString &url, bool active)> createFn,
                           std::function<bool(int tabId, const QString &url)> updateFn,
                           std::function<bool(int tabId)> removeFn);
    void setTabUrlLookup(std::function<QString(int tabId)> fn);
    void setScriptRunner(std::function<bool(int tabId, const QString &code,
                                             std::function<void(const QString &resultJson)> callback)> fn);

signals:
    void extensionsChanged(); // discovery, enable/disable, or reload happened

private:
    QWebEngineProfile *m_profile;
    QWebChannel *m_apiChannel;
    ChromeExtensionAPI *m_bridge;
    QVector<LoadedWebExtension> m_extensions;
    QString m_shimSource; // cached qwebchannel.js + chrome-api-shim.js text

    void unloadAll();
    void loadOne(const QString &dirPath);
    void injectContentScripts(const LoadedWebExtension &ext);
    void startBackgroundPage(LoadedWebExtension &ext);
    QString readFile(const QString &path) const;
    QString shimSource(); // lazily built, cached
    QString bootstrapWrapper(const QString &extensionId, const QString &contextId,
                              const QString &body, const QString &matchesJsonArray) const;
};
