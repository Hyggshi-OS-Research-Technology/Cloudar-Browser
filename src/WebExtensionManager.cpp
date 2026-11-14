#include "WebExtensionManager.h"
#include "ChromeExtensionAPI.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QSettings>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebChannel>

// --- tiny JS literal helpers (no JSON round-trip needed: we're generating
// JS source text directly, so plain string/array literals are enough) ---
static QString jsStringLiteral(const QString &s)
{
    QString out = s;
    out.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', "\\n");
    return '"' + out + '"';
}

static QString jsStringArrayLiteral(const QStringList &list)
{
    QStringList quoted;
    for (const QString &s : list)
        quoted << jsStringLiteral(s);
    return '[' + quoted.join(',') + ']';
}

static QString jsManifestObjectLiteral(const WebExtensionManifest &m)
{
    return QStringLiteral(
               "{\"name\":%1,\"version\":%2,\"description\":%3,\"permissions\":%4,\"hostPermissions\":%5}")
        .arg(jsStringLiteral(m.name), jsStringLiteral(m.version),
             jsStringLiteral(m.description), jsStringArrayLiteral(m.permissions),
             jsStringArrayLiteral(m.hostPermissions));
}

WebExtensionManager::WebExtensionManager(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent), m_profile(profile)
{
    m_apiChannel = new QWebChannel(this);
    m_bridge = new ChromeExtensionAPI(this);
    m_apiChannel->registerObject("chromeBridge", m_bridge);

    m_bridge->setPermissionLookup([this](const QString &extId) -> QStringList {
        for (const LoadedWebExtension &e : m_extensions) {
            if (e.id == extId)
                return e.manifest.permissions;
        }
        return {};
    });
    m_bridge->setHostPermissionLookup([this](const QString &extId) -> QStringList {
        for (const LoadedWebExtension &e : m_extensions) {
            if (e.id == extId)
                return e.manifest.hostPermissions;
        }
        return {};
    });

    QDir().mkpath(extensionsRootDir());
    reload();
}

QString WebExtensionManager::extensionsRootDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/webextensions";
}

void WebExtensionManager::setTabsProvider(std::function<QString()> provider)
{
    m_bridge->setTabsProvider(std::move(provider));
}

void WebExtensionManager::setTabController(std::function<int(const QString &, bool)> createFn,
                                            std::function<bool(int, const QString &)> updateFn,
                                            std::function<bool(int)> removeFn)
{
    m_bridge->setTabController(std::move(createFn), std::move(updateFn), std::move(removeFn));
}

void WebExtensionManager::setTabUrlLookup(std::function<QString(int)> fn)
{
    m_bridge->setTabUrlLookup(std::move(fn));
}

void WebExtensionManager::setScriptRunner(
    std::function<bool(int, const QString &, std::function<void(const QString &)>)> fn)
{
    m_bridge->setScriptRunner(std::move(fn));
}

QString WebExtensionManager::readFile(const QString &path) const
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}

QString WebExtensionManager::shimSource()
{
    if (!m_shimSource.isEmpty())
        return m_shimSource;

    // qwebchannel.js ships as a built-in Qt WebChannel resource (the same
    // one settings.html/newtab.html load via qrc:/qtwebchannel/qwebchannel.js)
    // — reused here so injected content scripts get a working QWebChannel
    // client without us vendoring a copy.
    const QString transport = readFile(":/qtwebchannel/qwebchannel.js");
    const QString apiShim = readFile(":/chrome-api-shim.js");
    m_shimSource = transport + "\n" + apiShim;
    return m_shimSource;
}

QString WebExtensionManager::bootstrapWrapper(const QString &extensionId, const QString &contextId,
                                               const QString &body, const QString &matchesJsonArray) const
{
    // Empty matches list = background context: no URL gating, and the
    // result is assigned to the real `window.chrome` since a background
    // page is entirely our own content, not a shared/ordinary web page.
    if (matchesJsonArray.isEmpty()) {
        return QStringLiteral(
                   "window.chrome = __lbExtGetAPI(%1, %2, %3);\n%4")
            .arg(jsStringLiteral(extensionId))
            .arg("__lbExtManifest_" + extensionId) // placeholder, replaced by caller before use
            .arg(jsStringLiteral(contextId))
            .arg(body);
    }
    return QStringLiteral(
               "(function(){\n"
               "  if (!__lbExtMatches(%1)) return;\n"
               "  var chrome = __lbExtGetAPI(%2, %3, %4);\n"
               "  %5\n"
               "})();")
        .arg(matchesJsonArray, jsStringLiteral(extensionId), "__MANIFEST__", jsStringLiteral(contextId), body);
}

void WebExtensionManager::unloadAll()
{
    QWebEngineScriptCollection *scripts = m_profile->scripts();
    for (const QWebEngineScript &s : scripts->find("webext:shim"))
        scripts->remove(s);
    for (const LoadedWebExtension &ext : m_extensions) {
        for (int i = 0; i < ext.manifest.contentScripts.size(); ++i) {
            const QString name = QStringLiteral("webext:content:%1:%2").arg(ext.id).arg(i);
            for (const QWebEngineScript &s : scripts->find(name))
                scripts->remove(s);
        }
        if (ext.backgroundPage)
            delete ext.backgroundPage;
        // Alarms and session storage are process-lifetime state tied to a
        // running background context — drop them along with it. storage.local
        // is left untouched; it's meant to survive reloads, like real Chrome.
        m_bridge->clearExtensionState(ext.id);
    }
    m_extensions.clear();
}

void WebExtensionManager::reload()
{
    unloadAll();

    // The shim (qwebchannel.js + chrome-api-shim.js) is inserted exactly
    // once per profile, at DocumentCreation in the shared extensions world,
    // so it exists before any individual content script runs.
    QWebEngineScriptCollection *scripts = m_profile->scripts();
    for (const QWebEngineScript &s : scripts->find("webext:shim"))
        scripts->remove(s);
    QWebEngineScript shim;
    shim.setName("webext:shim");
    shim.setSourceCode(shimSource());
    shim.setInjectionPoint(QWebEngineScript::DocumentCreation);
    shim.setWorldId(EXTENSIONS_WORLD_ID);
    shim.setRunsOnSubFrames(true);
    scripts->insert(shim);

    const QDir root(extensionsRootDir());
    const QStringList subdirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &sub : subdirs)
        loadOne(root.filePath(sub));

    emit extensionsChanged();
}

void WebExtensionManager::loadOne(const QString &dirPath)
{
    WebExtensionManifest manifest = WebExtensionManifest::parse(dirPath);
    if (!manifest.valid)
        return; // folder without a valid manifest.json is silently skipped

    LoadedWebExtension ext;
    ext.id = QDir(dirPath).dirName();
    ext.rootDir = dirPath;
    ext.manifest = manifest;

    QSettings settings;
    ext.enabled = settings.value("webextensions/enabled/" + ext.id, true).toBool();

    if (ext.enabled) {
        injectContentScripts(ext);
        startBackgroundPage(ext);
    }

    m_extensions.append(ext);
}

void WebExtensionManager::injectContentScripts(const LoadedWebExtension &ext)
{
    QWebEngineScriptCollection *scripts = m_profile->scripts();
    const QDir dir(ext.rootDir);
    const QString manifestLiteral = jsManifestObjectLiteral(ext.manifest);

    for (int i = 0; i < ext.manifest.contentScripts.size(); ++i) {
        const ContentScriptEntry &entry = ext.manifest.contentScripts.at(i);

        QString body;
        for (const QString &rel : entry.jsFiles)
            body += readFile(dir.filePath(rel)) + "\n";

        QString wrapped = bootstrapWrapper(ext.id, "content:" + ext.id, body, jsStringArrayLiteral(entry.matches));
        wrapped.replace("__MANIFEST__", manifestLiteral);

        QWebEngineScript script;
        script.setName(QStringLiteral("webext:content:%1:%2").arg(ext.id).arg(i));
        script.setSourceCode(wrapped);
        script.setWorldId(EXTENSIONS_WORLD_ID);
        script.setRunsOnSubFrames(entry.allFrames);
        if (entry.runAt == "document_start")
            script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        else if (entry.runAt == "document_end")
            script.setInjectionPoint(QWebEngineScript::DocumentReady);
        else
            script.setInjectionPoint(QWebEngineScript::Deferred); // ~= document_idle
        scripts->insert(script);
    }
}

void WebExtensionManager::startBackgroundPage(LoadedWebExtension &ext)
{
    if (ext.manifest.backgroundServiceWorker.isEmpty())
        return;

    const QDir dir(ext.rootDir);
    const QString scriptBody = readFile(dir.filePath(ext.manifest.backgroundServiceWorker));
    if (scriptBody.isEmpty())
        return;

    QString wrapped = bootstrapWrapper(ext.id, "background", scriptBody, QString());
    wrapped.replace("__lbExtManifest_" + ext.id, jsManifestObjectLiteral(ext.manifest));

    QWebEnginePage *page = new QWebEnginePage(m_profile, this);
    page->setWebChannel(m_apiChannel); // world 0 (MainWorld) — this page is entirely our own content

    const QString html = QStringLiteral(
                              "<!DOCTYPE html><html><head><script>%1</script></head>"
                              "<body></body></html>")
                              .arg(shimSource() + "\n" + wrapped);
    page->setHtml(html, QUrl::fromLocalFile(dir.absolutePath() + "/"));
    ext.backgroundPage = page;
}

void WebExtensionManager::attachToPage(QWebEnginePage *page)
{
    if (!page)
        return;
    page->setWebChannel(m_apiChannel, EXTENSIONS_WORLD_ID);
}

void WebExtensionManager::setEnabled(const QString &extensionId, bool enabled)
{
    QSettings settings;
    settings.setValue("webextensions/enabled/" + extensionId, enabled);
    reload();
}

bool WebExtensionManager::isEnabled(const QString &extensionId) const
{
    QSettings settings;
    return settings.value("webextensions/enabled/" + extensionId, true).toBool();
}
