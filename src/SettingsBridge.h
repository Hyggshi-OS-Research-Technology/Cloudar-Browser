#pragma once

#include <QObject>
#include <QString>

class QWebEngineProfile;
class QWidget;
class HistoryManager;
class MainWindow;

// Backs the cloudar://settings page (see resources/settings.html). Exposed on
// the shared QWebChannel as "settingsBridge" — registered only in
// MainWindow and, like NativeBridge, attached exclusively to cloudar:// pages
// (see BrowserTabWidget::wireView), so ordinary websites never see it.
//
// Every setter here writes straight to QSettings (the same store
// SettingsDialog reads/writes) and applies live where the app can react
// immediately (accent color and interface style via ThemeManager, cookie
// policy via the profile, memory-saver thresholds via the settingsChanged()
// signal MemorySaver listens to). Everything else (home page, search engine,
// downloads folder) is read fresh from QSettings the next time it is needed.
//
// A few settings physically cannot be applied to a live window: the interface
// style changes the window model (frameless vs decorated) and rebuilds the menu
// bar, and the performance tier changes Chromium's command line. Those are
// stored, reported back by restartRequired(), and applied on the next launch —
// the settings page says so next to each control rather than pretending.
class SettingsBridge : public QObject
{
    Q_OBJECT
public:
    explicit SettingsBridge(QWebEngineProfile *profile, HistoryManager *historyManager,
                             QWidget *parentWidget, QObject *parent = nullptr);

public slots:
    // Returns a JSON object (as a string) with every value the settings
    // page needs to render itself: homePage, searchEngine, persistCookies,
    // downloadDir, askDownloadLocation, accentColor, version.
    QString getAll() const;

    void setHomePage(const QString &url);
    void setRestoreSession(bool enabled);
    void setSearchEngine(const QString &templateUrl); // "%s" placeholder for the query
    void setPersistCookies(bool enabled);
    void setDownloadDir(const QString &dir);
    void setAskDownloadLocation(bool enabled);
    void setAccentColor(const QString &hex);

    // Native dialogs. Both block until the user closes them and return the
    // chosen value, or an empty string if cancelled — called from JS like
    // `bridge.browseDownloadsDir(cur, function(dir) { ... })`.
    QString browseDownloadsDir(const QString &startDir);
    QString pickColor(const QString &startHex);

    // `history`/`cache`/`cookies` are independently toggleable, mirroring
    // the checkboxes in the "Clear browsing data" panel.
    void clearBrowsingData(bool history, bool cache, bool cookies);

    // Tor settings (cloudar://settings/tor)
    void setTorEnabled(bool enabled);
    void setTorProxy(const QString &host, int port);
    void setTorOnionSupport(bool enabled);
    void setTorBlockWebRTC(bool enabled);
    void setTorAntiFingerprint(bool enabled);
    void setTorBridgeType(const QString &type);

    // Torrent settings (cloudar://settings/torrent)
    void setTorrentEnabled(bool enabled);
    void setTorrentDownloadDir(const QString &dir);
    void setTorrentAskLocation(bool enabled);
    void setTorrentMaxActive(int count);
    void setTorrentMaxDownloadSpeed(const QString &speed);
    void setTorrentMaxUploadSpeed(const QString &speed);
    void setTorrentSeedRatio(const QString &ratio);
    void setTorrentProtocol(const QString &key, bool enabled);

    // Performance & Memory Saver (light / medium / high / ultra)
    void setMemorySaverEnabled(bool enabled);
    void setMemorySaverLevel(const QString &level);
    // Discards every background tab's renderer through MemorySaver, clears
    // the HTTP cache and trims V8's caches. Returns the number of megabytes
    // actually reclaimed where it can be measured, and an estimate otherwise
    // (the settings page labels it as such).
    int cleanMemoryNow();

    // Honest live numbers for the settings page: tabs currently sleeping,
    // estimated bytes they held, and the CPU/RAM facts the tier was chosen
    // from. Kept as one JSON call so the page can refresh cheaply.
    QString performanceStats() const;

    // UI motion (accent glow, tab fade-in, sidebar slide). Disabled by default
    // on the Legacy CPU tier.
    bool animationsEnabled() const;
    void setAnimationsEnabled(bool enabled);

    // Hardware acceleration: toggling this requires a restart, and the
    // settings page says so.
    bool hardwareAccelerationEnabled() const;
    void setHardwareAccelerationEnabled(bool enabled);

    // "classic" (Chrome 2009) or "modern". Takes effect on restart because the
    // window model, tab placement and menu bar all change.
    QString uiStyle() const;

    // Colour mode for the modern chrome: "system" | "light" | "dark". Unlike the
    // interface style, this can apply to a live window — it changes colours, not
    // the window model — so setColorMode() repaints immediately.
    void setColorMode(const QString &mode);
    // True when this Qt build can observe the OS colour scheme at all. The page
    // uses it to explain what "System" actually means on an older Qt.
    bool colorSchemeQueryable() const;
    void setUiStyle(const QString &style);

    // Chrome 2009's two toolbar toggles: the Home button and the bookmarks bar.
    void setShowHomeButton(bool enabled);
    void setShowBookmarkBar(bool enabled);
    bool restartRequired() const;

    // Explicit CPU tier override: "auto" | "legacy" | "low" | "balanced" | "high".
    QString cpuTierSetting() const;
    void setCpuTierSetting(const QString &tier);
    QString cpuTierDetected() const;

    // Flags & Sidebar control (cloudar://flags)
    bool isSidebarEnabled() const;
    void setSidebarEnabled(bool enabled);
    QString getAllFlags() const;
    void setFlag(const QString &key, const QString &value);
    void resetAllFlags();
    void relaunch();

    QString appVersion() const;

signals:
    // Reserved for future cross-tab sync (e.g. two settings tabs open at
    // once); not currently emitted, but kept so the JS side already wires
    // a listener for it.
    void settingsChanged();

private:
    QWebEngineProfile *m_profile;
    HistoryManager *m_historyManager;
    QWidget *m_parentWidget;
};
