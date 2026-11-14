#include "ExtensionManager.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QSettings>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QRegularExpression>

ExtensionManager::ExtensionManager(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent), m_profile(profile)
{
    QDir().mkpath(extensionsDirPath());
    reload();
}

QString ExtensionManager::extensionsDirPath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/extensions";
    return dir;
}

QString ExtensionManager::parseHeaderField(const QString &source, const QString &field) const
{
    // Looks for a line like: // @name My Script
    QRegularExpression re("//\\s*@" + field + "\\s+(.+)");
    QRegularExpressionMatch m = re.match(source);
    if (m.hasMatch())
        return m.captured(1).trimmed();
    return QString();
}

QVector<UserScriptInfo> ExtensionManager::discover() const
{
    QVector<UserScriptInfo> result;
    QDir dir(extensionsDirPath());
    const QStringList files = dir.entryList(QStringList() << "*.js", QDir::Files, QDir::Name);

    QSettings settings;
    for (const QString &fileName : files) {
        QFile file(dir.filePath(fileName));
        QString content;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            content = QString::fromUtf8(file.readAll());

        UserScriptInfo info;
        info.fileName = fileName;
        info.name = parseHeaderField(content, "name");
        if (info.name.isEmpty())
            info.name = fileName;
        info.match = parseHeaderField(content, "match");
        if (info.match.isEmpty())
            info.match = "*";
        info.enabled = settings.value("extensions/" + fileName, true).toBool();
        result.append(info);
    }
    return result;
}

void ExtensionManager::setEnabled(const QString &fileName, bool enabled)
{
    QSettings settings;
    settings.setValue("extensions/" + fileName, enabled);
    reload();
}

bool ExtensionManager::isEnabled(const QString &fileName) const
{
    QSettings settings;
    return settings.value("extensions/" + fileName, true).toBool();
}

void ExtensionManager::reload()
{
    // Wipe and re-insert: simplest way to keep the injected set in sync
    // with what's enabled without tracking individual QWebEngineScript
    // identities.
    QWebEngineScriptCollection *scripts = m_profile->scripts();
    for (const UserScriptInfo &info : discover()) {
        // Remove any previous version of this script (by name) first.
        for (const QWebEngineScript &existing : scripts->find(info.name))
            scripts->remove(existing);
    }

    QDir dir(extensionsDirPath());
    for (const UserScriptInfo &info : discover()) {
        if (!info.enabled)
            continue;

        QFile file(dir.filePath(info.fileName));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QWebEngineScript script;
        script.setName(info.name);
        script.setSourceCode(QString::fromUtf8(file.readAll()));
        script.setInjectionPoint(QWebEngineScript::DocumentReady);
        script.setWorldId(QWebEngineScript::MainWorld);
        script.setRunsOnSubFrames(false);
        scripts->insert(script);
    }
}
