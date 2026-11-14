#ifndef BROWSER_RESOURCES_H
#define BROWSER_RESOURCES_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QMap>

namespace BrowserResources {

// Application root & data directories
QString getAppRootDir();
QString getBrowserDataDir();
QString getBackgroundsDir();

QString getBookmarksFile();
QString getHistoryFile();
QString getSettingsFile();
QString getDownloadsFile();
QString getPermissionsFile();
QString getFlagsFile();

// JSON helpers
QJsonDocument loadJsonDocument(const QString &filePath, const QJsonDocument &defaultDoc = QJsonDocument());
QJsonObject loadJsonObject(const QString &filePath, const QJsonObject &defaultObj = QJsonObject());
QJsonArray loadJsonArray(const QString &filePath, const QJsonArray &defaultArr = QJsonArray());
bool saveJsonDocument(const QString &filePath, const QJsonDocument &doc);
bool saveJsonObject(const QString &filePath, const QJsonObject &obj);
bool saveJsonArray(const QString &filePath, const QJsonArray &arr);

// Default configuration
QJsonObject getDefaultSettings();

// Tracking domains & anti-tracking scripts
const QSet<QString>& getTrackingDomains();
const QMap<QString, QString>& getUserAgents();
QString getAntiFingerprintingJs();
QString getAntiTrackingJs();

// Unicode icons
QString getIcon(const QString &name);

} // namespace BrowserResources

#endif // BROWSER_RESOURCES_H
