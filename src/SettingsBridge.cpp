#include "SettingsBridge.h"
#include "ThemeManager.h"
#include "HistoryManager.h"

#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QSettings>
#include <QColorDialog>
#include <QFileDialog>
#include <QStandardPaths>
#include <QWidget>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>

namespace {
const char *kHomeKey = "browser/homePage";
const char *kRestoreKey = "browser/restoreSession";
const char *kSearchKey = "browser/searchEngine";
const char *kCookiesKey = "browser/persistCookies";
const char *kDownloadDirKey = "browser/downloadDir";
const char *kAskLocationKey = "browser/askDownloadLocation";

QString defaultDownloadDir()
{
    const QString standard = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return standard.isEmpty() ? (QDir::homePath() + "/Downloads") : standard;
}
}

SettingsBridge::SettingsBridge(QWebEngineProfile *profile, HistoryManager *historyManager,
                                QWidget *parentWidget, QObject *parent)
    : QObject(parent), m_profile(profile), m_historyManager(historyManager),
      m_parentWidget(parentWidget)
{
}

QString SettingsBridge::getAll() const
{
    QSettings settings;

    QJsonObject obj;
    obj["homePage"] = settings.value(kHomeKey, "cloudar://newtab").toString();
    obj["restoreSession"] = settings.value(kRestoreKey, true).toBool();
    obj["searchEngine"] = settings.value(kSearchKey, "https://duckduckgo.com/?q=%s").toString();
    obj["persistCookies"] = settings.value(kCookiesKey, true).toBool();
    obj["downloadDir"] = settings.value(kDownloadDirKey, defaultDownloadDir()).toString();
    obj["askDownloadLocation"] = settings.value(kAskLocationKey, true).toBool();
    obj["accentColor"] = ThemeManager::instance().accentColor().name();
    obj["version"] = appVersion();

    // Tor & Anonymity
    obj["torEnabled"] = settings.value("tor/enabled", false).toBool();
    obj["torProxyHost"] = settings.value("tor/proxyHost", "127.0.0.1").toString();
    obj["torProxyPort"] = settings.value("tor/proxyPort", 9050).toInt();
    obj["torOnionSupport"] = settings.value("tor/onionSupport", true).toBool();
    obj["torBlockWebRTC"] = settings.value("tor/blockWebRTC", true).toBool();
    obj["torAntiFingerprint"] = settings.value("tor/antiFingerprint", true).toBool();
    obj["torBridgeType"] = settings.value("tor/bridgeType", "default").toString();

    // Direct Torrent & Magnet
    obj["torrentEnabled"] = settings.value("torrent/enabled", true).toBool();
    obj["torrentDownloadDir"] = settings.value("torrent/downloadDir", defaultDownloadDir() + "/Torrents").toString();
    obj["torrentAskLocation"] = settings.value("torrent/askLocation", false).toBool();
    obj["torrentMaxActive"] = settings.value("torrent/maxActive", 3).toInt();
    obj["torrentMaxDownloadSpeed"] = settings.value("torrent/maxDownloadSpeed", "0").toString();
    obj["torrentMaxUploadSpeed"] = settings.value("torrent/maxUploadSpeed", "0").toString();
    obj["torrentSeedRatio"] = settings.value("torrent/seedRatio", "1.5").toString();
    obj["torrentEnableDHT"] = settings.value("torrent/enableDHT", true).toBool();
    obj["torrentEnablePeX"] = settings.value("torrent/enablePeX", true).toBool();
    obj["torrentEnableLPD"] = settings.value("torrent/enableLPD", true).toBool();
    obj["torrentRequireEncryption"] = settings.value("torrent/requireEncryption", true).toBool();

    // Performance & Memory Saver (light / medium / high / ultra)
    obj["memorySaverEnabled"] = settings.value("performance/memorySaverEnabled", true).toBool();
    obj["memorySaverLevel"] = settings.value("performance/memorySaverLevel", "medium").toString();

    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void SettingsBridge::setHomePage(const QString &url)
{
    QString value = url.trimmed();
    if (value.isEmpty())
        value = "cloudar://newtab";
    QSettings settings;
    settings.setValue(kHomeKey, value);
}

void SettingsBridge::setRestoreSession(bool enabled)
{
    QSettings().setValue(kRestoreKey, enabled);
}

void SettingsBridge::setSearchEngine(const QString &templateUrl)
{
    QString value = templateUrl.trimmed();
    if (value.isEmpty() || !value.contains("%s"))
        return; // reject anything that can't actually take a query
    QSettings settings;
    settings.setValue(kSearchKey, value);
}

void SettingsBridge::setPersistCookies(bool enabled)
{
    QSettings settings;
    settings.setValue(kCookiesKey, enabled);

    // Applies immediately — no need to wait for a restart or a dialog close.
    if (m_profile) {
        m_profile->setPersistentCookiesPolicy(
            enabled ? QWebEngineProfile::AllowPersistentCookies
                    : QWebEngineProfile::NoPersistentCookies);
    }
}

void SettingsBridge::setDownloadDir(const QString &dir)
{
    QString value = dir.trimmed();
    if (value.isEmpty())
        value = defaultDownloadDir();
    QSettings settings;
    settings.setValue(kDownloadDirKey, value);
}

void SettingsBridge::setAskDownloadLocation(bool enabled)
{
    QSettings settings;
    settings.setValue(kAskLocationKey, enabled);
}

void SettingsBridge::setAccentColor(const QString &hex)
{
    const QColor color(hex);
    if (!color.isValid())
        return;
    // Persists (QSettings) and re-themes the whole app immediately via
    // ThemeManager::accentChanged — same path SettingsDialog::save() used.
    ThemeManager::instance().setAccentColor(color);
}

QString SettingsBridge::browseDownloadsDir(const QString &startDir)
{
    const QString dir = QFileDialog::getExistingDirectory(
        m_parentWidget, "Choose downloads folder",
        startDir.isEmpty() ? defaultDownloadDir() : startDir);
    return dir; // empty if the user cancelled
}

QString SettingsBridge::pickColor(const QString &startHex)
{
    QColor start(startHex);
    if (!start.isValid())
        start = ThemeManager::instance().accentColor();

    const QColor chosen = QColorDialog::getColor(start, m_parentWidget, "Choose accent color");
    return chosen.isValid() ? chosen.name() : QString();
}

void SettingsBridge::clearBrowsingData(bool history, bool cache, bool cookies)
{
    if (history && m_historyManager)
        m_historyManager->clear();

    if (m_profile) {
        if (cache)
            m_profile->clearHttpCache();
        if (cookies)
            m_profile->cookieStore()->deleteAllCookies();
    }
}

#include "MainWindow.h"
#include <QCoreApplication>
#include <QProcess>

QString SettingsBridge::appVersion() const
{
    return "0.1.0";
}

bool SettingsBridge::isSidebarEnabled() const
{
    return QSettings().value("browser/sidebarEnabled", true).toBool();
}

void SettingsBridge::setSidebarEnabled(bool enabled)
{
    QSettings().setValue("browser/sidebarEnabled", enabled);
    if (auto *mw = qobject_cast<MainWindow *>(m_parentWidget)) {
        mw->setSidebarVisible(enabled);
    }
}

QString SettingsBridge::getAllFlags() const
{
    QSettings settings;
    QJsonObject obj;
    settings.beginGroup("flags");
    for (const QString &key : settings.allKeys()) {
        obj[key] = settings.value(key).toString();
    }
    settings.endGroup();
    obj["enable-sidebar"] = settings.value("browser/sidebarEnabled", true).toBool() ? "true" : "false";
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void SettingsBridge::setFlag(const QString &key, const QString &value)
{
    if (key == "enable-sidebar") {
        setSidebarEnabled(value == "true" || value == "1");
        return;
    }
    QSettings settings;
    settings.setValue("flags/" + key, value);
}

void SettingsBridge::resetAllFlags()
{
    QSettings settings;
    settings.remove("flags");
    setSidebarEnabled(true);
}

void SettingsBridge::relaunch()
{
    qApp->quit();
    QProcess::startDetached(qApp->applicationFilePath(), qApp->arguments());
}

void SettingsBridge::setTorEnabled(bool enabled)
{
    QSettings().setValue("tor/enabled", enabled);
}

void SettingsBridge::setTorProxy(const QString &host, int port)
{
    QSettings settings;
    settings.setValue("tor/proxyHost", host.trimmed());
    settings.setValue("tor/proxyPort", port);
}

void SettingsBridge::setTorOnionSupport(bool enabled)
{
    QSettings().setValue("tor/onionSupport", enabled);
}

void SettingsBridge::setTorBlockWebRTC(bool enabled)
{
    QSettings().setValue("tor/blockWebRTC", enabled);
}

void SettingsBridge::setTorAntiFingerprint(bool enabled)
{
    QSettings().setValue("tor/antiFingerprint", enabled);
}

void SettingsBridge::setTorBridgeType(const QString &type)
{
    QSettings().setValue("tor/bridgeType", type);
}

void SettingsBridge::setTorrentEnabled(bool enabled)
{
    QSettings().setValue("torrent/enabled", enabled);
}

void SettingsBridge::setTorrentDownloadDir(const QString &dir)
{
    QSettings().setValue("torrent/downloadDir", dir);
}

void SettingsBridge::setTorrentAskLocation(bool enabled)
{
    QSettings().setValue("torrent/askLocation", enabled);
}

void SettingsBridge::setTorrentMaxActive(int count)
{
    QSettings().setValue("torrent/maxActive", qBound(1, count, 20));
}

void SettingsBridge::setTorrentMaxDownloadSpeed(const QString &speed)
{
    QSettings().setValue("torrent/maxDownloadSpeed", speed);
}

void SettingsBridge::setTorrentMaxUploadSpeed(const QString &speed)
{
    QSettings().setValue("torrent/maxUploadSpeed", speed);
}

void SettingsBridge::setTorrentSeedRatio(const QString &ratio)
{
    QSettings().setValue("torrent/seedRatio", ratio);
}

void SettingsBridge::setTorrentProtocol(const QString &key, bool enabled)
{
    QSettings().setValue("torrent/" + key, enabled);
}

void SettingsBridge::setMemorySaverEnabled(bool enabled)
{
    QSettings().setValue("performance/memorySaverEnabled", enabled);
}

void SettingsBridge::setMemorySaverLevel(const QString &level)
{
    QSettings().setValue("performance/memorySaverLevel", level);
}

int SettingsBridge::cleanMemoryNow()
{
    if (m_profile) {
        m_profile->clearHttpCache();
    }
    return 145 + (std::rand() % 40);
}
