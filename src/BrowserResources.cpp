#include "BrowserResources.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

namespace BrowserResources {

QString getAppRootDir() {
    static QString cachedRootDir;
    if (!cachedRootDir.isEmpty()) {
        return cachedRootDir;
    }

#ifdef CLOUDAR_SOURCE_DIR
    QDir srcDir(CLOUDAR_SOURCE_DIR);
    if (srcDir.exists("internal_pages")) {
        cachedRootDir = srcDir.absolutePath();
        return cachedRootDir;
    }
#endif

    QString appDir = QCoreApplication::applicationDirPath();
    if (QDir(appDir).exists("internal_pages")) {
        cachedRootDir = appDir;
        return cachedRootDir;
    }

    QDir parentDir(appDir);
    if (parentDir.cdUp() && parentDir.exists("internal_pages")) {
        cachedRootDir = parentDir.absolutePath();
        return cachedRootDir;
    }

    QDir currentDir(QDir::currentPath());
    if (currentDir.exists("internal_pages")) {
        cachedRootDir = currentDir.absolutePath();
        return cachedRootDir;
    }

    cachedRootDir = appDir;
    return cachedRootDir;
}

QString getBrowserDataDir() {
    QString root = getAppRootDir();
    QString dataDir = root + "/browser_data";
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dataDir;
}

QString getBackgroundsDir() {
    QString bgDir = getBrowserDataDir() + "/backgrounds";
    QDir dir(bgDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return bgDir;
}

QString getBookmarksFile() {
    return getBrowserDataDir() + "/bookmarks.json";
}

QString getHistoryFile() {
    return getBrowserDataDir() + "/history.json";
}

QString getSettingsFile() {
    return getBrowserDataDir() + "/settings.json";
}

QString getDownloadsFile() {
    return getBrowserDataDir() + "/downloads.json";
}

QString getPermissionsFile() {
    return getBrowserDataDir() + "/permissions.json";
}

QString getFlagsFile() {
    return getBrowserDataDir() + "/flags.json";
}

QJsonDocument loadJsonDocument(const QString &filePath, const QJsonDocument &defaultDoc) {
    QFile file(filePath);
    if (!file.exists()) {
        return defaultDoc;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open JSON file for reading:" << filePath;
        return defaultDoc;
    }
    QByteArray data = file.readAll();
    file.close();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Error parsing JSON in" << filePath << ":" << error.errorString();
        return defaultDoc;
    }
    return doc;
}

QJsonObject loadJsonObject(const QString &filePath, const QJsonObject &defaultObj) {
    QJsonDocument doc = loadJsonDocument(filePath, QJsonDocument(defaultObj));
    return doc.isObject() ? doc.object() : defaultObj;
}

QJsonArray loadJsonArray(const QString &filePath, const QJsonArray &defaultArr) {
    QJsonDocument doc = loadJsonDocument(filePath, QJsonDocument(defaultArr));
    return doc.isArray() ? doc.array() : defaultArr;
}

bool saveJsonDocument(const QString &filePath, const QJsonDocument &doc) {
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Failed to open JSON file for writing:" << filePath;
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool saveJsonObject(const QString &filePath, const QJsonObject &obj) {
    return saveJsonDocument(filePath, QJsonDocument(obj));
}

bool saveJsonArray(const QString &filePath, const QJsonArray &arr) {
    return saveJsonDocument(filePath, QJsonDocument(arr));
}

QJsonObject getDefaultSettings() {
    QJsonObject s;
    s["home_page"] = "cloudar://newtab";
    s["search_engine"] = "https://www.google.com/search?q={}";
    s["download_location"] = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    s["theme"] = "dark";
    s["language"] = "";
    s["show_bookmark_bar"] = false;
    s["vertical_tabs"] = false;
    s["vertical_tabs_collapsed"] = true;
    s["translation_enabled"] = true;
    s["third_party_cookies"] = "block";
    s["safe_browsing"] = true;
    s["ad_privacy"] = true;
    s["adblock_enabled"] = true;
    s["newtab_frame_color"] = "#60cdff";
    s["newtab_background_image"] = "";
    s["performance_memory_saver"] = true;
    s["performance_energy_saver"] = true;
    s["performance_hardware_acceleration"] = true;
    s["tab_sleep_minutes"] = 5;
    s["force_download_directory"] = false;
    s["forced_download_path"] = "";
    s["privacy_mode"] = "standard";
    s["auto_delete_cookies_on_exit"] = false;
    s["block_fingerprinting"] = true;
    s["block_tracking_scripts"] = true;
    s["webrtc_ip_protection"] = true;
    s["referrer_policy"] = "strict-origin-when-cross-origin";
    s["user_agent_mode"] = "default";
    s["custom_user_agent"] = "";
    s["timezone_spoof"] = false;
    s["language_spoof"] = false;
    s["privacy_quick_action"] = false;
    s["block_third_party_ads"] = true;
    s["ask_download"] = false;
    s["startup_behavior"] = "Open New Tab";
    return s;
}

const QSet<QString>& getTrackingDomains() {
    static const QSet<QString> domains = {
        "google-analytics.com",
        "doubleclick.net",
        "googlesyndication.com",
        "googletagmanager.com",
        "googletagservices.com",
        "googleadservices.com",
        "adservice.google.com",
        "adservice.google.co.uk",
        "facebook.net",
        "connect.facebook.net",
        "analytics.twitter.com",
        "ads-twitter.com",
        "ads.linkedin.com",
        "bat.bing.com",
        "pixel.quantserve.com",
        "scorecardresearch.com",
        "pagead2.googlesyndication.com",
        "ad.doubleclick.net",
        "securepubads.g.doubleclick.net",
        "cdn.segment.com",
        "api.segment.io",
        "amplitude.com",
        "cdn.heapanalytics.com",
        "cdn.mxpnl.com",
        "hotjar.com",
        "static.hotjar.com",
        "script.hotjar.com",
        "cdn.cookielaw.org",
        "cmp.quantcast.com",
        "onesignal.com",
        "cdn.pushalert.co",
        "t.co",
        "ads.yahoo.com",
        "yieldmo.com",
        "adnxs.com",
        "adsrvr.org",
        "criteo.com",
        "criteo.net",
        "casalemedia.com",
        "openx.net",
        "pubmatic.com",
        "rubiconproject.com",
        "sharethrough.com",
        "spotxchange.com",
        "taboola.com",
        "trc.taboola.com",
        "outbrain.com",
        "widgets.outbrain.com",
        "amazon-adsystem.com",
        "aax.amazon-adsystem.com",
        "clarity.ms",
        "adroll.com",
        "mediamath.com",
        "gumgum.com",
        "indexww.com",
        "smartadserver.com",
        "teads.tv",
        "adcolony.com",
        "vungle.com",
        "unityads.unity3d.com"
    };
    return domains;
}

const QMap<QString, QString>& getUserAgents() {
    static const QMap<QString, QString> uas = {
        {"chrome", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"},
        {"firefox", "Mozilla/5.0 (X11; Linux x86_64; rv:120.0) Gecko/20100101 Firefox/120.0"},
        {"edge", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0"}
    };
    return uas;
}

QString getAntiFingerprintingJs() {
    return QStringLiteral(R"(
(function() {
    const originalToDataURL = HTMLCanvasElement.prototype.toDataURL;
    const originalToBlob = HTMLCanvasElement.prototype.toBlob;
    const originalGetImageData = CanvasRenderingContext2D.prototype.getImageData;

    HTMLCanvasElement.prototype.toDataURL = function() {
        const result = originalToDataURL.apply(this, arguments);
        if (!this._protected) {
            this._protected = true;
            const ctx = this.getContext('2d');
            if (ctx) {
                const imageData = ctx.getImageData(0, 0, 1, 1);
                imageData.data[0] = Math.min(255, Math.max(0, imageData.data[0] + (Math.random() > 0.5 ? 1 : -1)));
                imageData.data[1] = Math.min(255, Math.max(0, imageData.data[1] + (Math.random() > 0.5 ? 1 : -1)));
                imageData.data[2] = Math.min(255, Math.max(0, imageData.data[2] + (Math.random() > 0.5 ? 1 : -1)));
                ctx.putImageData(imageData, 0, 0);
            }
            const modified = originalToDataURL.apply(this, arguments);
            this._protected = false;
            return modified;
        }
        return result;
    };

    function addNoiseToWebGL() {
        try {
            const getParameter = WebGLRenderingContext.prototype.getParameter;
            WebGLRenderingContext.prototype.getParameter = function(pname) {
                if (pname === 0x1F01 || pname === 0x1F00) {
                    return 'Mesa DRI Intel(R) HD Graphics (anon)';
                }
                if (pname === 0x1F02) {
                    return 'WebGL 1.0 (OpenGL ES 2.0 Chromium)';
                }
                return getParameter.apply(this, arguments);
            };

            const getParameter2 = WebGL2RenderingContext.prototype.getParameter;
            WebGL2RenderingContext.prototype.getParameter = function(pname) {
                if (pname === 0x1F01 || pname === 0x1F00) {
                    return 'Mesa DRI Intel(R) HD Graphics (anon)';
                }
                return getParameter2.apply(this, arguments);
            };
        } catch(e) {}
    }
    addNoiseToWebGL();

    try {
        Object.defineProperty(navigator, 'deviceMemory', { get: () => 8 });
        const origHardwareConcurrency = Object.getOwnPropertyDescriptor(
            Object.getPrototypeOf(navigator), 'hardwareConcurrency'
        );
        if (!origHardwareConcurrency || origHardwareConcurrency.configurable) {
            Object.defineProperty(navigator, 'hardwareConcurrency', { get: () => 4 });
        }
    } catch(e) {}
})();
)");
}

QString getAntiTrackingJs() {
    return QStringLiteral(R"(
(function() {
    const trackingPatterns = [
        "google-analytics.com", "doubleclick.net", "googlesyndication.com",
        "googletagmanager.com", "facebook.net", "connect.facebook.net",
        "analytics.twitter.com", "hotjar.com", "criteo.com", "taboola.com", "outbrain.com"
    ];

    const originalFetch = window.fetch;
    window.fetch = function(input, init) {
        const url = typeof input === 'string' ? input : (input.url || '');
        if (trackingPatterns.some(p => url.includes(p))) {
            return Promise.reject(new Error('Blocked by tracking protection'));
        }
        return originalFetch.apply(this, arguments);
    };

    const originalOpen = XMLHttpRequest.prototype.open;
    XMLHttpRequest.prototype.open = function(method, url) {
        if (trackingPatterns.some(p => String(url).includes(p))) {
            return;
        }
        return originalOpen.apply(this, arguments);
    };
})();
)");
}

QString getIcon(const QString &name) {
    static const QMap<QString, QString> iconMap = {
        {"back", QString::fromUtf8("\u2190")},
        {"forward", QString::fromUtf8("\u2192")},
        {"reload", QString::fromUtf8("\u21bb")},
        {"home", QString::fromUtf8("\u2302")},
        {"stop", QString::fromUtf8("\u2715")},
        {"bookmark", QString::fromUtf8("\u2605")},
        {"bookmark_empty", QString::fromUtf8("\u2606")},
        {"new_tab", "+"},
        {"close", QString::fromUtf8("\u2715")},
        {"menu", QString::fromUtf8("\u22ee")},
        {"download", QString::fromUtf8("\u2b07")},
        {"history", QString::fromUtf8("\u23f2")},
        {"settings", QString::fromUtf8("\u2699")},
        {"lock", QString::fromUtf8("\U0001f512")},
        {"unlock", QString::fromUtf8("\U0001f513")},
        {"play", QString::fromUtf8("\u25b6")},
        {"pause", QString::fromUtf8("\u23f8")},
        {"skip_next", QString::fromUtf8("\u23ed")},
        {"skip_prev", QString::fromUtf8("\u23ee")},
        {"media_note", QString::fromUtf8("\U0001f3b5")},
        {"translate", QString::fromUtf8("\u6587")},
        {"incognito", QString::fromUtf8("\U0001f575")},
        {"privacy", QString::fromUtf8("\U0001f6e1")},
        {"fingerprint", QString::fromUtf8("\U0001f9be")},
        {"ghost", QString::fromUtf8("\U0001f47b")}
    };
    return iconMap.value(name, name);
}

} // namespace BrowserResources
