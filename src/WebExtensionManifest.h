#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// One `content_scripts` entry from manifest.json. `matches` uses the same
// syntax Chrome does (e.g. "*://*.example.com/*", "<all_urls>") — matched
// against page URLs at runtime by chrome-api-shim.js, since QWebEngineScript
// itself has no concept of per-script URL matching.
struct ContentScriptEntry {
    QStringList matches;
    QStringList jsFiles;      // paths relative to the extension's root dir
    QString runAt = "document_idle"; // document_start | document_end | document_idle
    bool allFrames = false;
};

// Parsed, validated subset of a Chrome Manifest V3 manifest.json. Only the
// fields LightBrowser's extension runtime actually understands are kept —
// unknown fields are ignored rather than rejected, so real-world extensions
// that use unsupported features (webRequest, devtools, side panels, ...)
// still *load*, they just won't get those particular capabilities.
struct WebExtensionManifest {
    bool valid = false;
    QString error; // human-readable reason when valid == false

    QString name;
    QString version;
    QString description;
    int manifestVersion = 0;

    QStringList permissions;      // "storage", "tabs", "activeTab", ...
    QStringList hostPermissions;  // MV3 host_permissions (URL match patterns)

    QString backgroundServiceWorker; // relative path; empty = no background context
    QVector<ContentScriptEntry> contentScripts;

    QString actionDefaultTitle;
    QString actionDefaultIcon; // single relative path (icon-size maps are simplified away)

    // Reads and parses <extensionDir>/manifest.json. Returns a manifest with
    // valid == false and error set if the file is missing, isn't valid JSON,
    // or is missing required fields (name, version, manifest_version).
    static WebExtensionManifest parse(const QString &extensionDir);

    bool hasPermission(const QString &permission) const;
};
