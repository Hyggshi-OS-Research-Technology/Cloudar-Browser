#include "WebExtensionAPIBridge.h"

#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

WebExtensionAPIBridge::WebExtensionAPIBridge(QObject *parent)
    : QObject(parent)
{
}

void WebExtensionAPIBridge::setPermissionLookup(std::function<QStringList(const QString &)> fn)
{
    m_permissionLookup = std::move(fn);
}

void WebExtensionAPIBridge::setTabsProvider(std::function<QString()> fn)
{
    m_tabsProvider = std::move(fn);
}

QString WebExtensionAPIBridge::storageGroup(const QString &extensionId) const
{
    // Sanitize: extensionId is derived from a folder name by
    // WebExtensionManager, but never trust it blindly when it ends up in a
    // QSettings key path.
    QString safe = extensionId;
    safe.replace('/', '_').replace('\\', '_');
    return "webextensions/" + safe + "/storage";
}

QString WebExtensionAPIBridge::storageGet(const QString &extensionId, const QString &keysJson) const
{
    QSettings settings;
    settings.beginGroup(storageGroup(extensionId));
    const QStringList allKeys = settings.childKeys();

    QStringList wanted;
    const QJsonDocument keysDoc = QJsonDocument::fromJson(keysJson.toUtf8());
    if (keysDoc.isArray()) {
        for (const QJsonValue &v : keysDoc.array())
            wanted << v.toString();
    } else {
        wanted = allKeys; // null/omitted keys => everything
    }

    QJsonObject result;
    for (const QString &key : wanted) {
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
    return QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
}

void WebExtensionAPIBridge::storageSet(const QString &extensionId, const QString &itemsJson)
{
    const QJsonDocument doc = QJsonDocument::fromJson(itemsJson.toUtf8());
    if (!doc.isObject())
        return;

    QSettings settings;
    settings.beginGroup(storageGroup(extensionId));
    const QJsonObject obj = doc.object();
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

void WebExtensionAPIBridge::storageRemove(const QString &extensionId, const QString &keysJson)
{
    const QJsonDocument doc = QJsonDocument::fromJson(keysJson.toUtf8());
    if (!doc.isArray())
        return;

    QSettings settings;
    settings.beginGroup(storageGroup(extensionId));
    for (const QJsonValue &v : doc.array())
        settings.remove(v.toString());
    settings.endGroup();
}

void WebExtensionAPIBridge::storageClear(const QString &extensionId)
{
    QSettings settings;
    settings.beginGroup(storageGroup(extensionId));
    settings.remove(""); // removes every key under this group
    settings.endGroup();
}

void WebExtensionAPIBridge::runtimeSendMessage(const QString &extensionId, const QString &senderContextId,
                                                const QString &messageJson)
{
    emit messageReceived(extensionId, senderContextId, messageJson);
}

bool WebExtensionAPIBridge::hasPermission(const QString &extensionId, const QString &permission) const
{
    if (!m_permissionLookup)
        return false;
    return m_permissionLookup(extensionId).contains(permission);
}

QString WebExtensionAPIBridge::tabsQuery(const QString & /*extensionId*/) const
{
    // Query filters (active/currentWindow/url/...) aren't implemented —
    // callers get every open tab back and can filter client-side. Real
    // per-extension "tabs" permission gating happens by simply not exposing
    // chrome.tabs at all when the manifest lacks the permission (see
    // chrome-api-shim.js).
    if (!m_tabsProvider)
        return "[]";
    return m_tabsProvider();
}
