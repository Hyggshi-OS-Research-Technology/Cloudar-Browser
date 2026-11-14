#include "WebExtensionManifest.h"

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

static QStringList jsonArrayToStringList(const QJsonArray &arr)
{
    QStringList out;
    for (const QJsonValue &v : arr) {
        if (v.isString())
            out << v.toString();
    }
    return out;
}

WebExtensionManifest WebExtensionManifest::parse(const QString &extensionDir)
{
    WebExtensionManifest m;

    QFile file(QDir(extensionDir).filePath("manifest.json"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m.error = "manifest.json not found or unreadable";
        return m;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        m.error = "manifest.json is not valid JSON: " + parseError.errorString();
        return m;
    }

    const QJsonObject root = doc.object();

    m.name = root.value("name").toString();
    m.version = root.value("version").toString();
    m.description = root.value("description").toString();
    m.manifestVersion = root.value("manifest_version").toInt(0);

    if (m.name.isEmpty() || m.version.isEmpty() || m.manifestVersion == 0) {
        m.error = "manifest.json missing required field (name, version, manifest_version)";
        return m;
    }

    m.permissions = jsonArrayToStringList(root.value("permissions").toArray());
    m.hostPermissions = jsonArrayToStringList(root.value("host_permissions").toArray());
    // MV2-style extensions sometimes list URL patterns straight in
    // "permissions" instead of a separate "host_permissions" array — anything
    // in there that looks like a match pattern also counts as a host permission.
    for (const QString &p : m.permissions) {
        if (p.contains("://") || p == "<all_urls>")
            m.hostPermissions << p;
    }

    const QJsonObject background = root.value("background").toObject();
    m.backgroundServiceWorker = background.value("service_worker").toString();
    if (m.backgroundServiceWorker.isEmpty())
        m.backgroundServiceWorker = background.value("scripts").toArray().first().toString();

    const QJsonArray contentScripts = root.value("content_scripts").toArray();
    for (const QJsonValue &v : contentScripts) {
        if (!v.isObject())
            continue;
        const QJsonObject obj = v.toObject();
        ContentScriptEntry entry;
        entry.matches = jsonArrayToStringList(obj.value("matches").toArray());
        entry.jsFiles = jsonArrayToStringList(obj.value("js").toArray());
        entry.runAt = obj.value("run_at").toString("document_idle");
        entry.allFrames = obj.value("all_frames").toBool(false);
        if (!entry.matches.isEmpty() && !entry.jsFiles.isEmpty())
            m.contentScripts.append(entry);
    }

    // MV3 uses "action", MV2 used "browser_action" — accept either.
    QJsonObject action = root.value("action").toObject();
    if (action.isEmpty())
        action = root.value("browser_action").toObject();
    m.actionDefaultTitle = action.value("default_title").toString();
    const QJsonValue icon = action.value("default_icon");
    if (icon.isString())
        m.actionDefaultIcon = icon.toString();
    else if (icon.isObject() && !icon.toObject().isEmpty())
        m.actionDefaultIcon = icon.toObject().begin().value().toString();

    m.valid = true;
    return m;
}

bool WebExtensionManifest::hasPermission(const QString &permission) const
{
    return permissions.contains(permission);
}
