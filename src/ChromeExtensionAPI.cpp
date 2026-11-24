#include "ChromeExtensionAPI.h"

#include <QSettings>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>
#include <algorithm>

ChromeExtensionAPI::ChromeExtensionAPI(QObject *parent)
    : QObject(parent)
{
}

void ChromeExtensionAPI::setPermissionLookup(std::function<QStringList(const QString &)> fn)
{
    m_permissionLookup = std::move(fn);
}

void ChromeExtensionAPI::setHostPermissionLookup(std::function<QStringList(const QString &)> fn)
{
    m_hostPermissionLookup = std::move(fn);
}

void ChromeExtensionAPI::setTabsProvider(std::function<QString()> fn)
{
    m_tabsProvider = std::move(fn);
}

void ChromeExtensionAPI::setTabController(std::function<int(const QString &, bool)> createFn,
                                           std::function<bool(int, const QString &)> updateFn,
                                           std::function<bool(int)> removeFn)
{
    m_tabCreate = std::move(createFn);
    m_tabUpdate = std::move(updateFn);
    m_tabRemove = std::move(removeFn);
}

void ChromeExtensionAPI::setTabUrlLookup(std::function<QString(int)> fn)
{
    m_tabUrlLookup = std::move(fn);
}

void ChromeExtensionAPI::setScriptRunner(
    std::function<bool(int, const QString &, std::function<void(const QString &)>)> fn)
{
    m_scriptRunner = std::move(fn);
}

void ChromeExtensionAPI::clearExtensionState(const QString &extensionId)
{
    alarmsClearAll(extensionId);
    m_sessionStorage.remove(extensionId);
}

// --- storage -----------------------------------------------------------

QString ChromeExtensionAPI::storageGroup(const QString &extensionId, const QString & /*area*/) const
{
    // Sanitize: extensionId is derived from a folder name, but never trust
    // it blindly when it ends up in a QSettings key path.
    QString safe = extensionId;
    safe.replace('/', '_').replace('\\', '_');
    return "webextensions/" + safe + "/storage";
}

QString ChromeExtensionAPI::storageGet(const QString &extensionId, const QString &area,
                                        const QString &keysJson) const
{
    QStringList wanted;
    const QJsonDocument keysDoc = QJsonDocument::fromJson(keysJson.toUtf8());
    const bool wantAll = !keysDoc.isArray();
    if (!wantAll) {
        for (const QJsonValue &v : keysDoc.array())
            wanted << v.toString();
    }

    QJsonObject result;

    if (area == "session") {
        const QHash<QString, QString> store = m_sessionStorage.value(extensionId);
        const QStringList keys = wantAll ? store.keys() : wanted;
        for (const QString &key : keys) {
            if (!store.contains(key))
                continue;
            const QJsonDocument envelope = QJsonDocument::fromJson(store.value(key).toUtf8());
            if (envelope.isObject())
                result.insert(key, envelope.object().value("v"));
        }
    } else {
        QSettings settings;
        settings.beginGroup(storageGroup(extensionId, area));
        const QStringList allKeys = settings.childKeys();
        const QStringList keys = wantAll ? allKeys : wanted;
        for (const QString &key : keys) {
            if (!allKeys.contains(key))
                continue;
            // Every stored value is wrapped as {"v": <original value>} (see
            // storageSet) so scalars, arrays, objects, and booleans all
            // round-trip through QSettings without ambiguity.
            const QString raw = settings.value(key).toString();
            const QJsonDocument envelope = QJsonDocument::fromJson(raw.toUtf8());
            if (envelope.isObject())
                result.insert(key, envelope.object().value("v"));
        }
        settings.endGroup();
    }

    return QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
}

void ChromeExtensionAPI::storageSet(const QString &extensionId, const QString &area,
                                     const QString &itemsJson)
{
    const QJsonDocument doc = QJsonDocument::fromJson(itemsJson.toUtf8());
    if (!doc.isObject())
        return;
    const QJsonObject obj = doc.object();

    if (area == "session") {
        QHash<QString, QString> &store = m_sessionStorage[extensionId];
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            QJsonObject envelope;
            envelope.insert("v", it.value());
            store.insert(it.key(), QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
        }
        return;
    }

    QSettings settings;
    settings.beginGroup(storageGroup(extensionId, area));
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        // Wrap every value as {"v": <value>} regardless of type, so a
        // scalar like `5` and an array like `[5]` can never be confused
        // when read back (see storageGet).
        QJsonObject envelope;
        envelope.insert("v", it.value());
        settings.setValue(it.key(), QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
    }
    settings.endGroup();
}

void ChromeExtensionAPI::storageRemove(const QString &extensionId, const QString &area,
                                        const QString &keysJson)
{
    const QJsonDocument doc = QJsonDocument::fromJson(keysJson.toUtf8());
    if (!doc.isArray())
        return;

    if (area == "session") {
        QHash<QString, QString> &store = m_sessionStorage[extensionId];
        for (const QJsonValue &v : doc.array())
            store.remove(v.toString());
        return;
    }

    QSettings settings;
    settings.beginGroup(storageGroup(extensionId, area));
    for (const QJsonValue &v : doc.array())
        settings.remove(v.toString());
    settings.endGroup();
}

void ChromeExtensionAPI::storageClear(const QString &extensionId, const QString &area)
{
    if (area == "session") {
        m_sessionStorage.remove(extensionId);
        return;
    }

    QSettings settings;
    settings.beginGroup(storageGroup(extensionId, area));
    settings.remove(""); // removes every key under this group
    settings.endGroup();
}

// --- runtime messaging ---------------------------------------------------

void ChromeExtensionAPI::runtimeSendMessage(const QString &extensionId, const QString &senderContextId,
                                             const QString &messageJson)
{
    emit messageReceived(extensionId, senderContextId, messageJson);
}

// --- permissions ---------------------------------------------------------

bool ChromeExtensionAPI::hasPermission(const QString &extensionId, const QString &permission) const
{
    if (!m_permissionLookup)
        return false;
    return m_permissionLookup(extensionId).contains(permission);
}

// Chrome match-pattern matching: scheme://host/path with "*" wildcards and
// the "<all_urls>" shorthand. Mirrored from chrome-api-shim.js's
// __lbExtPatternTest so host-permission checks are enforced the same way
// on both sides — JS gates what's *offered*, this gates what's actually
// *allowed*, and the two need to agree.
bool ChromeExtensionAPI::urlMatchesPattern(const QString &pattern, const QString &url)
{
    if (pattern == QLatin1String("<all_urls>")) {
        static const QRegularExpression schemeRe(QStringLiteral("^(https?|file|ftp):"));
        return schemeRe.match(url).hasMatch();
    }

    static const QRegularExpression patternRe(
        QStringLiteral("^(\\*|https?|file|ftp)://(\\*|(?:\\*\\.)?[^/]+)(/.*)$"));
    const QRegularExpressionMatch pm = patternRe.match(pattern);
    if (!pm.hasMatch())
        return false;
    const QString scheme = pm.captured(1);
    const QString host = pm.captured(2);
    const QString path = pm.captured(3);

    static const QRegularExpression urlRe(QStringLiteral("^([a-zA-Z]+)://([^/]*)(/.*)?$"));
    const QRegularExpressionMatch um = urlRe.match(url);
    if (!um.hasMatch())
        return false;
    const QString uScheme = um.captured(1);
    const QString uHost = um.captured(2);
    QString uPath = um.captured(3);
    if (uPath.isEmpty())
        uPath = "/";

    if (scheme != "*" && scheme.compare(uScheme, Qt::CaseInsensitive) != 0)
        return false;

    if (host != "*") {
        if (host.startsWith("*.")) {
            const QString base = host.mid(2);
            if (uHost != base && !uHost.endsWith("." + base))
                return false;
        } else if (host != uHost) {
            return false;
        }
    }

    QStringList escapedParts;
    for (const QString &part : path.split('*'))
        escapedParts << QRegularExpression::escape(part);
    const QRegularExpression pathRe("^" + escapedParts.join(".*") + "$");
    return pathRe.match(uPath).hasMatch();
}

bool ChromeExtensionAPI::hasHostPermission(const QString &extensionId, const QString &url) const
{
    if (!m_hostPermissionLookup)
        return false;
    const QStringList patterns = m_hostPermissionLookup(extensionId);
    return std::any_of(patterns.begin(), patterns.end(), [&url](const QString &pattern) {
        return urlMatchesPattern(pattern, url);
    });
}

// --- tabs ------------------------------------------------------------------

QString ChromeExtensionAPI::tabsQuery(const QString & /*extensionId*/) const
{
    // Query filters (active/currentWindow/url/...) aren't implemented —
    // callers get every open tab back and filter client-side. Real
    // per-extension "tabs" permission gating happens both here (see the
    // other tabs* methods) and by chrome-api-shim.js simply not exposing
    // chrome.tabs at all when the manifest lacks the permission.
    if (!m_tabsProvider)
        return "[]";
    return m_tabsProvider();
}

int ChromeExtensionAPI::tabsCreate(const QString &extensionId, const QString &url, bool active)
{
    if (!hasPermission(extensionId, "tabs") || !m_tabCreate)
        return -1;
    return m_tabCreate(url, active);
}

bool ChromeExtensionAPI::tabsUpdate(const QString &extensionId, int tabId, const QString &url)
{
    if (!hasPermission(extensionId, "tabs") || !m_tabUpdate)
        return false;
    return m_tabUpdate(tabId, url);
}

bool ChromeExtensionAPI::tabsRemove(const QString &extensionId, int tabId)
{
    if (!hasPermission(extensionId, "tabs") || !m_tabRemove)
        return false;
    return m_tabRemove(tabId);
}

// --- scripting ---------------------------------------------------------

void ChromeExtensionAPI::scriptingExecute(const QString &extensionId, const QString &contextId,
                                           int tabId, const QString &code, const QString &requestId)
{
    bool allowed = hasPermission(extensionId, "scripting") && m_scriptRunner;
    if (allowed) {
        const QString tabUrl = m_tabUrlLookup ? m_tabUrlLookup(tabId) : QString();
        allowed = hasPermission(extensionId, "activeTab")
                  || (!tabUrl.isEmpty() && hasHostPermission(extensionId, tabUrl));
    }

    if (!allowed) {
        emit scriptExecuted(extensionId, contextId, requestId, QStringLiteral("{\"error\":\"not permitted\"}"));
        return;
    }

    const QString id = extensionId; // captured by value: lambda may outlive this call
    m_scriptRunner(tabId, code, [this, id, contextId, requestId](const QString &resultJson) {
        emit scriptExecuted(id, contextId, requestId, resultJson);
    });
}

// --- alarms --------------------------------------------------------------

void ChromeExtensionAPI::alarmsCreate(const QString &extensionId, const QString &name,
                                       double delayInMinutes, double periodInMinutes)
{
    if (!hasPermission(extensionId, "alarms"))
        return;

    alarmsClear(extensionId, name); // replaces any existing alarm of the same name, like Chrome

    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);

    connect(timer, &QTimer::timeout, this, [this, extensionId, name]() {
        emit alarmFired(extensionId, name);
        auto extIt = m_alarms.find(extensionId);
        if (extIt == m_alarms.end())
            return;
        auto entryIt = extIt->find(name);
        if (entryIt == extIt->end())
            return;
        if (entryIt->periodInMinutes > 0.0) {
            entryIt->timer->start(static_cast<int>((std::max)(1.0, entryIt->periodInMinutes) * 60000.0));
        } else {
            entryIt->timer->deleteLater();
            extIt->remove(name);
        }
    });

    m_alarms[extensionId].insert(name, AlarmEntry{timer, periodInMinutes});
    timer->start(static_cast<int>((std::max)(0.0, delayInMinutes) * 60000.0));
}

void ChromeExtensionAPI::alarmsClear(const QString &extensionId, const QString &name)
{
    auto extIt = m_alarms.find(extensionId);
    if (extIt == m_alarms.end())
        return;
    auto entryIt = extIt->find(name);
    if (entryIt == extIt->end())
        return;
    entryIt->timer->stop();
    entryIt->timer->deleteLater();
    extIt->remove(name);
}

void ChromeExtensionAPI::alarmsClearAll(const QString &extensionId)
{
    auto extIt = m_alarms.find(extensionId);
    if (extIt == m_alarms.end())
        return;
    for (AlarmEntry &entry : extIt.value()) {
        entry.timer->stop();
        entry.timer->deleteLater();
    }
    m_alarms.remove(extensionId);
}

QString ChromeExtensionAPI::alarmsGetAll(const QString &extensionId) const
{
    QJsonArray arr;
    const auto extIt = m_alarms.find(extensionId);
    if (extIt != m_alarms.end()) {
        for (auto nameIt = extIt->begin(); nameIt != extIt->end(); ++nameIt) {
            QJsonObject o;
            o["name"] = nameIt.key();
            o["periodInMinutes"] = nameIt->periodInMinutes;
            arr.append(o);
        }
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

// --- action ----------------------------------------------------------------

void ChromeExtensionAPI::actionSetBadgeText(const QString &extensionId, const QString &text)
{
    m_badgeText[extensionId] = text;
    emit badgeChanged(extensionId, text, m_badgeColor.value(extensionId));
}

void ChromeExtensionAPI::actionSetBadgeBackgroundColor(const QString &extensionId, const QString &color)
{
    m_badgeColor[extensionId] = color;
    emit badgeChanged(extensionId, m_badgeText.value(extensionId), color);
}

void ChromeExtensionAPI::actionSetTitle(const QString &extensionId, const QString &title)
{
    emit titleChanged(extensionId, title);
}

void ChromeExtensionAPI::notifyActionClicked(const QString &extensionId, int tabId)
{
    emit actionClicked(extensionId, tabId);
}
