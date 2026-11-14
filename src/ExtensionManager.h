#pragma once

#include <QObject>
#include <QVector>
#include <QWebEngineProfile>

struct UserScriptInfo {
    QString fileName;   // e.g. "dark-mode.js"
    QString name;       // parsed from "// @name ..." header, falls back to fileName
    QString match;      // parsed from "// @match ..." header, "*" = all sites
    bool enabled;
};

// A minimal Greasemonkey-style extension system: any .js file dropped into
// the extensions folder can be toggled on/off and gets injected into pages
// via QWebEngineScript. No sandboxing, no manifest.json, no permissions
// model — deliberately simple, matching the "light" philosophy of this
// browser. Good for small personal tweaks (ad-hoc dark mode, autofill
// helpers, small DOM patches), not a full extension store.
class ExtensionManager : public QObject
{
    Q_OBJECT
public:
    explicit ExtensionManager(QWebEngineProfile *profile, QObject *parent = nullptr);

    QString extensionsDirPath() const;
    QVector<UserScriptInfo> discover() const;

    void setEnabled(const QString &fileName, bool enabled);
    bool isEnabled(const QString &fileName) const;

    void reload(); // re-scan folder and re-apply enabled scripts to the profile

private:
    QWebEngineProfile *m_profile;

    QString parseHeaderField(const QString &source, const QString &field) const;
};
