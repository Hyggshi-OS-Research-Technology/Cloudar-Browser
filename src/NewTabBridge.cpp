#include "NewTabBridge.h"

#include "BookmarkManager.h"
#include "ClassicTheme.h"
#include "FaviconStore.h"
#include "HistoryManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace {

const char *kSearchKey = "browser/searchEngine";
const char *kDefaultSearch = "https://duckduckgo.com/?q=%s";
// Wallpaper preference. Deliberately in QSettings next to every other browser
// preference, not in the page's localStorage: see saveWallpaperMedia().
const char *kWallpaperKindKey = "browser/newTabWallpaper/kind";
const char *kWallpaperValueKey = "browser/newTabWallpaper/value";
const char *kWallpaperFileKey = "browser/newTabWallpaper/file";

QString wallpaperDir()
{
    // AppDataLocation, same root MainWindow uses for the WebEngine profile, so
    // everything the browser owns sits under one directory that uninstalling
    // removes. It is NOT under the WebEngine storage path: clearing browsing
    // data must not delete a wallpaper.
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dir = base + QStringLiteral("/wallpaper");
    QDir().mkpath(dir);
    return dir;
}

// The page shows the engine's name above the search box; deriving it from the
// template beats a second setting that can disagree with the first one.
QString engineName(const QString &searchTemplate)
{
    const QString host = QUrl(searchTemplate).host().toLower();
    if (host.contains(QLatin1String("duckduckgo")))
        return QStringLiteral("DuckDuckGo");
    if (host.contains(QLatin1String("google")))
        return QStringLiteral("Google");
    if (host.contains(QLatin1String("bing")))
        return QStringLiteral("Bing");
    if (host.contains(QLatin1String("brave")))
        return QStringLiteral("Brave Search");
    if (host.contains(QLatin1String("startpage")))
        return QStringLiteral("Startpage");
    if (host.contains(QLatin1String("ecosia")))
        return QStringLiteral("Ecosia");
    if (host.contains(QLatin1String("qwant")))
        return QStringLiteral("Qwant");
    if (host.contains(QLatin1String("mojeek")))
        return QStringLiteral("Mojeek");
    if (host.contains(QLatin1String("wikipedia")))
        return QStringLiteral("Wikipedia");
    const QString bare = host.startsWith(QLatin1String("www.")) ? host.mid(4) : host;
    // Never invent a name: an unknown engine is reported as its hostname, which
    // is a fact, or as "the configured engine" when the template has no host.
    return bare.isEmpty() ? QStringLiteral("the configured engine") : bare;
}

} // namespace

NewTabBridge::NewTabBridge(BookmarkManager *bookmarks, HistoryManager *history,
                           FaviconStore *favicons, QObject *parent)
    : QObject(parent), m_bookmarks(bookmarks), m_history(history), m_favicons(favicons)
{
    if (m_favicons) {
        connect(m_favicons, &FaviconStore::faviconReady,
                this, &NewTabBridge::faviconReady, Qt::UniqueConnection);
    }
}

QVariantMap NewTabBridge::initialState()
{
    QVariantMap out;

    // The style in force for *this run*, not the saved preference: --classic
    // must repaint the New Tab page too, and Theme::current() is the only
    // source that knows about the command-line override.
    out["chromeStyle"] = Theme::keyFor(Theme::current());

    const QString searchTemplate =
            QSettings().value(QLatin1String(kSearchKey), QLatin1String(kDefaultSearch)).toString();
    out["searchTemplate"] = searchTemplate;
    out["searchName"] = engineName(searchTemplate);
    out["version"] = QCoreApplication::applicationVersion().isEmpty()
            ? QStringLiteral(CLOUDAR_VERSION)
            : QCoreApplication::applicationVersion();

    QVariantList bookmarks;
    if (m_bookmarks) {
        for (const BookmarkEntry &entry : m_bookmarks->entries()) {
            QVariantMap obj;
            obj["title"] = entry.title;
            obj["url"] = entry.url;
            bookmarks.append(obj);
        }
    }
    out["bookmarks"] = bookmarks;

    QVariantList recent;
    if (m_history) {
        // HistoryManager keeps entries newest-first (addVisit prepends), so this
        // is a straight walk — no reversal, which would surface the oldest pages.
        const QVector<HistoryEntry> entries = m_history->entries();
        const int cap = qMin(6, entries.size());
        for (int i = 0; i < cap; ++i) {
            const HistoryEntry &entry = entries[i];
            QVariantMap obj;
            obj["title"] = entry.title;
            obj["url"] = entry.url;
            obj["visited"] = entry.visitedAt.isValid() ? entry.visitedAt.toString(Qt::ISODate)
                                                       : QString();
            recent.append(obj);
        }
    }
    out["recent"] = recent;

    // What the wallpaper is and where its bytes live. The page uses this instead
    // of trusting its own localStorage, which is what made a saved wallpaper
    // vanish after a restart.
    out["wallpaper"] = wallpaperState();

    return out;
}

QString NewTabBridge::faviconUrl(const QString &pageUrl)
{
    if (!m_favicons)
        return QString();
    const QUrl url = QUrl::fromUserInput(pageUrl);
    if (!url.isValid())
        return QString();
    return m_favicons->faviconFor(url);
}

QString NewTabBridge::saveWallpaperMedia(const QString &kind, const QString &base64,
                                         const QString &value)
{
    QSettings settings;
    const QString normalised = kind.trimmed().toLower();

    if (normalised == QLatin1String("color") || normalised == QLatin1String("web")) {
        // Nothing to store on disk: a colour and a URL are a few bytes.
        clearWallpaperMedia();
        settings.setValue(QLatin1String(kWallpaperKindKey), normalised);
        settings.setValue(QLatin1String(kWallpaperValueKey), value);
        settings.remove(QLatin1String(kWallpaperFileKey));
        return QString();
    }

    if (normalised != QLatin1String("image") && normalised != QLatin1String("video"))
        return QString();

    const QByteArray bytes = QByteArray::fromBase64(base64.toLatin1());
    if (bytes.isEmpty())
        return QString();

    // One slot per kind ("wallpaper.jpg" / "wallpaper.mp4"), so choosing ten
    // wallpapers in a row cannot grow a directory of orphans. The extension is
    // part of the name so the scheme handler can answer with the right MIME type
    // without sniffing.
    const QString fileName = QStringLiteral("wallpaper.")
            + (normalised == QLatin1String("video") ? QStringLiteral("mp4")
                                                    : QStringLiteral("jpg"));
    const QString path = wallpaperDir() + QLatin1Char('/') + fileName;

    // Replace, do not append: QFile::WriteOnly would otherwise leave the tail of
    // a larger previous file behind when the new one is shorter.
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    if (out.write(bytes) != bytes.size()) {
        out.close();
        return QString();
    }
    out.close();

    // Exactly one media file on disk at a time.
    for (const QFileInfo &stale : QDir(wallpaperDir()).entryInfoList(QDir::Files)) {
        if (stale.absoluteFilePath() != path)
            QFile::remove(stale.absoluteFilePath());
    }

    settings.setValue(QLatin1String(kWallpaperKindKey), normalised);
    settings.setValue(QLatin1String(kWallpaperFileKey), path);
    settings.remove(QLatin1String(kWallpaperValueKey));
    return wallpaperMediaUrl();
}

QString NewTabBridge::wallpaperMediaUrl()
{
    const QSettings settings;
    const QString kind = settings.value(QLatin1String(kWallpaperKindKey)).toString();
    if (kind != QLatin1String("image") && kind != QLatin1String("video"))
        return QString();

    const QString path = settings.value(QLatin1String(kWallpaperFileKey)).toString();
    if (path.isEmpty() || !QFileInfo::exists(path))
        return QString();

    // The file name is a constant, so the URL is too — no need to encode a path
    // or leak the user's home directory into a page-visible string.
    return QStringLiteral("cloudar://newtab/media/") + QFileInfo(path).fileName();
}

QVariantMap NewTabBridge::wallpaperState()
{
    const QSettings settings;
    QVariantMap out;
    QString kind = settings.value(QLatin1String(kWallpaperKindKey)).toString();
    if (kind.isEmpty())
        kind = QStringLiteral("default");

    out["kind"] = kind;
    out["value"] = settings.value(QLatin1String(kWallpaperValueKey)).toString();
    out["url"] = wallpaperMediaUrl();

    // Real size on disk. Reporting 0 for a colour or URL wallpaper is accurate:
    // those consume no file.
    qint64 bytes = 0;
    const QString path = settings.value(QLatin1String(kWallpaperFileKey)).toString();
    if (!path.isEmpty()) {
        const QFileInfo info(path);
        if (info.exists())
            bytes = info.size();
    }
    out["bytes"] = static_cast<qlonglong>(bytes);
    return out;
}

void NewTabBridge::clearWallpaperMedia()
{
    QSettings settings;
    const QString path = settings.value(QLatin1String(kWallpaperFileKey)).toString();
    if (!path.isEmpty())
        QFile::remove(path);
    settings.remove(QLatin1String(kWallpaperFileKey));
}

void NewTabBridge::openUrl(const QString &url)
{
    const QUrl target = QUrl::fromUserInput(url.trimmed());
    if (!target.isValid() || target.isEmpty())
        return;
    emit navigationRequested(target);
}
