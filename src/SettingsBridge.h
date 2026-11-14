#pragma once

#include <QObject>
#include <QString>

class QWebEngineProfile;
class QWidget;
class HistoryManager;

// Backs the cloudar://settings page (see resources/settings.html). Exposed on
// the shared QWebChannel as "settingsBridge" — registered only in
// MainWindow and, like NativeBridge, attached exclusively to cloudar:// pages
// (see BrowserTabWidget::wireView), so ordinary websites never see it.
//
// Every setter here writes straight to QSettings (the same store
// SettingsDialog reads/writes) and applies live where the app can react
// immediately (accent color via ThemeManager, cookie policy via the
// profile). Everything else (home page, search engine, downloads folder)
// is read fresh from QSettings the next time it's needed, exactly as it
// was before this page existed.
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
    int cleanMemoryNow();

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
