#include "SettingsBridge.h"
#include "ThemeManager.h"
#include "HistoryManager.h"
#include "MainWindow.h"
#include "MemorySaver.h"
#include "PerformanceProfile.h"
#include "ClassicTheme.h"

#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QSettings>
#include <QColorDialog>
#include <QFileDialog>
#include <QStandardPaths>
#include <QWidget>
#include <QDir>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSysInfo>
#include <QProcess>

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

    // CPU / rendering compatibility
    obj["animationsEnabled"] = animationsEnabled();
    obj["hardwareAcceleration"] = hardwareAccelerationEnabled();
    obj["cpuTierSetting"] = cpuTierSetting();
    obj["cpuTierDetected"] = cpuTierDetected();
    obj["cpuSummary"] = PerformanceProfile::summary();
    obj["renderMode"] = PerformanceProfile::renderMode() == PerformanceProfile::RenderMode::Software
                            ? "software" : "accelerated";
    obj["chromiumFlags"] = PerformanceProfile::activeFlagSummary();

    // Appearance
    // Two different questions, two different answers: "uiStyle" is what the
    // next launch will use (the control's value), "uiStyleRunning" is what this
    // window is actually built from (used to reskin the page itself). They
    // differ whenever the user has picked a style but not restarted, or launched
    // with --classic/--modern.
    obj["uiStyle"] = uiStyle();
    obj["uiStyleRunning"] = Theme::keyFor(Theme::current());
    obj["showHomeButton"] = settings.value("browser/showHomeButton", true).toBool();
    obj["restartRequired"] = restartRequired();
    obj["showBookmarkBar"] = settings.value("browser/bookmarkBarVisible", true).toBool();
    obj["platform"] = QSysInfo::prettyProductName();
    obj["arch"] = QSysInfo::currentCpuArchitecture();
    obj["qtVersion"] = QString::fromLatin1(qVersion());
    obj["chromiumVersion"] = QWebEngineProfile::qWebEngineChromiumVersion();

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

QString SettingsBridge::appVersion() const
{
    // Single source of truth is main.cpp's setApplicationVersion(); reading it
    // back here keeps the About page from lying after a version bump.
    return QCoreApplication::applicationVersion();
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
    // Two pools, in the order that returns the most first:
    //   1. background renderers — MemorySaver tears them down entirely,
    //   2. the profile's HTTP cache and visited-link store.
    // The returned figure is an estimate for the renderers (QtWebEngine exposes
    // no per-tab RSS); the settings page labels it accordingly.
    int mb = 0;
    if (auto *mw = qobject_cast<MainWindow *>(m_parentWidget)) {
        if (MemorySaver *saver = mw->memorySaver())
            mb += saver->discardAllBackgroundTabs();
    }

    if (m_profile) {
        m_profile->clearHttpCache();
        m_profile->clearAllVisitedLinks();
    }

    emit settingsChanged();
    return mb;
}

QString SettingsBridge::performanceStats() const
{
    QJsonObject obj;
    const auto info = PerformanceProfile::cpuInfo();
    obj["logicalCores"] = info.logicalCores;
    obj["physicalCores"] = info.physicalCores;
    obj["ramMb"] = static_cast<double>(info.ramMb);
    obj["tierDetected"] = cpuTierDetected();
    obj["tierLabel"] = PerformanceProfile::tierLabel(PerformanceProfile::tier());
    obj["tierSetting"] = cpuTierSetting();
    obj["animationsEnabled"] = animationsEnabled();
    obj["renderMode"] = PerformanceProfile::renderMode() == PerformanceProfile::RenderMode::Software
                            ? "software" : "accelerated";
    obj["summary"] = PerformanceProfile::summary();
    obj["chromiumFlags"] = PerformanceProfile::activeFlagSummary();

    if (auto *mw = qobject_cast<MainWindow *>(m_parentWidget)) {
        if (MemorySaver *saver = mw->memorySaver()) {
            obj["sleptTabs"] = saver->sleptTabCount();
            obj["releasedMbEstimate"] =
                static_cast<double>(saver->approximatedFreedBytes() / (1024.0 * 1024.0));
            obj["thresholdMinutes"] = saver->thresholdMs() / 60000;
        }
    }
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

bool SettingsBridge::animationsEnabled() const
{
    return PerformanceProfile::animationsEnabled();
}

void SettingsBridge::setAnimationsEnabled(bool enabled)
{
    PerformanceProfile::setAnimationsEnabled(enabled);
    emit settingsChanged();
}

bool SettingsBridge::hardwareAccelerationEnabled() const
{
    return QSettings().value("performance/hardwareAcceleration", true).toBool();
}

void SettingsBridge::setHardwareAccelerationEnabled(bool enabled)
{
    QSettings().setValue("performance/hardwareAcceleration", enabled);
    emit settingsChanged();
}

QString SettingsBridge::uiStyle() const
{
    // The *saved* preference, so this control is not confused by a launch-time
    // --classic / --modern / CLOUDAR_THEME override; restartRequired() is what
    // reports "the running chrome is not the one you chose".
    return Theme::keyFor(Theme::savedStyle());
}

void SettingsBridge::setUiStyle(const QString &style)
{
    Theme::ThemeStyle parsed;
    if (!Theme::styleFromKey(style, &parsed))
        return;
    // Save only. An interface style is not a skin: it decides the window model
    // (frameless vs. decorated), which tab surface exists and which menu bar is
    // built, none of which can be swapped on a living window. Applying the new
    // stylesheet alone would leave a half-classic window, so the change lands on
    // the next launch and restartRequired() tells the settings page to say so.
    Theme::setCurrent(parsed);
    emit settingsChanged();
}

QString SettingsBridge::cpuTierSetting() const
{
    const QString value = QSettings().value("performance/cpuTier").toString();
    return value.isEmpty() ? QStringLiteral("auto") : value;
}

void SettingsBridge::setCpuTierSetting(const QString &tier)
{
    PerformanceProfile::setForcedTier(tier);
    emit settingsChanged();
}

void SettingsBridge::setShowHomeButton(bool enabled)
{
    QSettings().setValue("browser/showHomeButton", enabled);
    emit settingsChanged();
}

void SettingsBridge::setShowBookmarkBar(bool enabled)
{
    QSettings().setValue("browser/bookmarkBarVisible", enabled);
    emit settingsChanged();
}

bool SettingsBridge::restartRequired() const
{
    // Two settings change something that is fixed for the lifetime of a process:
    //   - the interface style decides the window model (frameless vs.
    //     decorated), the tab surface and which menu bar is built,
    //   - the performance tier is baked into QTWEBENGINE_CHROMIUM_FLAGS.
    // If either has changed since startup, the settings page says a restart is
    // pending and offers the button, instead of quietly doing nothing.
    QSettings settings;

    // The running chrome against the saved preference. No timestamp or snapshot
    // is needed: Theme::current() is what this process built its window from and
    // Theme::savedStyle() is what the next launch will use. A launch-time
    // --classic/--modern override also shows up here, which is correct — the
    // next plain launch will look different.
    if (Theme::current() != Theme::savedStyle())
        return true;

    const QString tierAtStartup = settings.value("performance/cpuTierAtStartup").toString();
    if (tierAtStartup.isEmpty())
        return false; // launched by a build that did not record it

    // The tier that is actually in effect now, including a --perf-tier override.
    return tierAtStartup != PerformanceProfile::tierKey(PerformanceProfile::tier());
}

QString SettingsBridge::cpuTierDetected() const
{
    // The hardware-only answer, ignoring any saved or environment override, so
    // the settings page can show "detected: X" next to "in use: Y".
    return PerformanceProfile::tierKey(PerformanceProfile::detectTierFromHardware());
}
