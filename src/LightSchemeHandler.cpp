#include "LightSchemeHandler.h"

#include "FaviconStore.h"
#include "Language.h"

#include <QWebEngineUrlScheme>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>

void LightSchemeHandler::registerScheme()
{
    for (const QByteArray &name : {QByteArray("cloudar"), QByteArray("light"), QByteArray("browser")}) {
        QWebEngineUrlScheme scheme(name);
        scheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                        QWebEngineUrlScheme::LocalAccessAllowed |
                        QWebEngineUrlScheme::CorsEnabled);
        scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
        QWebEngineUrlScheme::registerScheme(scheme);
    }
}

LightSchemeHandler::LightSchemeHandler(QObject *parent) : QWebEngineUrlSchemeHandler(parent) {}

void LightSchemeHandler::serveResource(QWebEngineUrlRequestJob *job, const QString &resourcePath,
                                        const QByteArray &mimeType)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    auto *buffer = new QBuffer(job);
    buffer->setData(file.readAll());
    buffer->open(QIODevice::ReadOnly);
    job->reply(mimeType, buffer);
}

void LightSchemeHandler::serveBytes(QWebEngineUrlRequestJob *job, const QByteArray &data,
                                    const QByteArray &mimeType)
{
    auto *buffer = new QBuffer(job);
    buffer->setData(data);
    buffer->open(QIODevice::ReadOnly);
    job->reply(mimeType, buffer);
}

void LightSchemeHandler::serveNewTabAsset(QWebEngineUrlRequestJob *job, const QString &path)
{
    // The New Tab page was split out of one 1100-line file into styles/ and
    // scripts/, and those files are served from the resource system under their
    // own paths so the page keeps readable URLs (assets/base.css) instead of one
    // flat qrc prefix. `path` arrives already normalised by QUrl, but it is still
    // checked: "../" or a leading "/" would otherwise let a page name any
    // resource in the binary.
    QString clean = path;
    while (clean.startsWith(QLatin1Char('/')))
        clean.remove(0, 1);
    if (clean.isEmpty() || clean.contains(QLatin1String(".."))
        || clean.contains(QLatin1Char('\\'))) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    const QString resource = QStringLiteral(":/newtab/") + clean;
    QByteArray mimeType = "application/octet-stream";
    if (clean.endsWith(QLatin1String(".css")))
        mimeType = "text/css; charset=utf-8";
    else if (clean.endsWith(QLatin1String(".js")))
        mimeType = "text/javascript; charset=utf-8";
    else if (clean.endsWith(QLatin1String(".html")))
        mimeType = "text/html; charset=utf-8";
    else if (clean.endsWith(QLatin1String(".svg")))
        mimeType = "image/svg+xml";
    else if (clean.endsWith(QLatin1String(".png")))
        mimeType = "image/png";

    serveResource(job, resource, mimeType);
}

void LightSchemeHandler::serveFavicon(QWebEngineUrlRequestJob *job)
{
    // Real icons that Chromium decoded and FaviconStore kept on disk. The page
    // passes the *page* URL (?u=...), not a host, so the store can apply the same
    // normalisation it uses when it records an icon.
    if (!m_favicons) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    const QUrlQuery query(job->requestUrl());
    const QString raw = query.queryItemValue(QStringLiteral("u"), QUrl::FullyDecoded);
    QByteArray data;
    QByteArray mimeType;
    if (raw.isEmpty() || !m_favicons->bytesForPageUrl(QUrl(raw), &data, &mimeType)) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    serveBytes(job, data, mimeType);
}

void LightSchemeHandler::serveWallpaperMedia(QWebEngineUrlRequestJob *job, const QString &path)
{
    // Only the names NewTabBridge ever writes are served, and only the still or
    // video formats it can write: wallpaper.<ext>. Anything else under /media/ is
    // a 404, so a page cannot use this host to read arbitrary files by guessing
    // names — the same reason the assets handler rejects "..".
    //
    // The extension is not decoration. The page uploads whatever format the user
    // picked (PNG keeps transparency, so it is deliberately not re-encoded), and
    // the stored file is named after it. Answering image/jpeg for PNG bytes was
    // only survivable because Chromium sniffs image content.
    static const QHash<QString, QByteArray> kTypes = {
        {QStringLiteral("jpg"),  QByteArrayLiteral("image/jpeg")},
        {QStringLiteral("jpeg"), QByteArrayLiteral("image/jpeg")},
        {QStringLiteral("png"),  QByteArrayLiteral("image/png")},
        {QStringLiteral("webp"), QByteArrayLiteral("image/webp")},
        {QStringLiteral("gif"),  QByteArrayLiteral("image/gif")},
        {QStringLiteral("svg"),  QByteArrayLiteral("image/svg+xml")},
        {QStringLiteral("mp4"),  QByteArrayLiteral("video/mp4")},
        {QStringLiteral("webm"), QByteArrayLiteral("video/webm")},
        {QStringLiteral("ogv"),  QByteArrayLiteral("video/ogg")},
    };

    const QString fileName = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0 || !fileName.startsWith(QLatin1String("wallpaper."))) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    const QString suffix = fileName.mid(dot + 1).toLower();
    if (!kTypes.contains(suffix)) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    // Read the *setting*, not the directory listing: a stale file left on disk
    // must not be served for a wallpaper the user has already replaced.
    const QSettings settings;
    const QString stored = settings.value(
                QStringLiteral("browser/newTabWallpaper/file")).toString();
    if (stored.isEmpty() || !QFileInfo::exists(stored)
        || QFileInfo(stored).fileName() != fileName) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    serveResource(job, stored, kTypes.value(suffix));
}

void LightSchemeHandler::serveLanguageDictionary(QWebEngineUrlRequestJob *job)
{
    LanguageManager *language = LanguageManager::instance();

    // The page gets one flat object: every English key mapped to the best text
    // available for it. Building it here rather than shipping the raw
    // translation file means a page can never render an empty label for a key
    // that happens to be missing from that language.
    QJsonObject english;
    QFile englishFile(QStringLiteral(":/lang/en.json"));
    if (englishFile.open(QIODevice::ReadOnly))
        english = QJsonDocument::fromJson(englishFile.readAll()).object();

    QJsonObject merged;
    const QString code = language->getCurrentLanguage();
    QJsonObject target;
    if (code != QLatin1String("en")) {
        QFile targetFile(QStringLiteral(":/lang/%1.json").arg(code));
        if (targetFile.open(QIODevice::ReadOnly))
            target = QJsonDocument::fromJson(targetFile.readAll()).object();
    }
    for (auto it = english.constBegin(); it != english.constEnd(); ++it) {
        const QString translated = target.value(it.key()).toString();
        merged.insert(it.key(), translated.isEmpty() ? it.value().toString() : translated);
    }
    // The resolved code itself, so a page can set <html lang> and format dates
    // without a second round trip.
    merged.insert(QStringLiteral("_language"), code);
    merged.insert(QStringLiteral("_preference"), language->preference());

    serveBytes(job, QJsonDocument(merged).toJson(QJsonDocument::Compact),
               "application/json; charset=utf-8");
}

void LightSchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    // Language assets are path-based and answered for every internal host, so
    // each page can request them same-origin. Checked before the host switch
    // because the New Tab page, the settings page and the manager pages are all
    // different origins from each other.
    if (job->requestUrl().path() == QLatin1String("/i18n.json")) {
        serveLanguageDictionary(job);
        return;
    }
    if (job->requestUrl().path() == QLatin1String("/i18n.js")) {
        serveResource(job, QStringLiteral(":/i18n.js"), "application/javascript; charset=utf-8");
        return;
    }

    // Hosts are matched case-insensitively, so cloudar://Settings/ and
    // cloudar://settings/ both resolve — the requested URL's casing and any
    // path/section after the host (e.g. cloudar://settings/privacy) are left
    // intact for the page itself; settings.html reads location.pathname to
    // deep-link to a section.
    const QString host = job->requestUrl().host().toLower();

    if (host == "newtab" || host == "new" || host.isEmpty()) {
        const QString path = job->requestUrl().path();
        // cloudar://newtab/assets/styles/base.css and friends. Same origin as the
        // page, which is why the favicon below is served from here too.
        if (path.startsWith(QLatin1String("/assets/"))) {
            serveNewTabAsset(job, path.mid(8));
            return;
        }
        // cloudar://newtab/favicon?u=<page url>
        if (path == QLatin1String("/favicon")) {
            serveFavicon(job);
            return;
        }
        // cloudar://newtab/media/wallpaper.jpg|.mp4 — survives restarts because
        // it lives in AppDataLocation, not in the renderer's storage partition.
        if (path.startsWith(QLatin1String("/media/"))) {
            serveWallpaperMedia(job, path);
            return;
        }
        serveResource(job, ":/newtab/index.html", "text/html; charset=utf-8");
    } else if (host == "favicon") {
        // Also reachable as its own host so an internal page that is not the New
        // Tab page can use it without knowing about the newtab host.
        serveFavicon(job);
    } else if (host == "settings") {
        serveResource(job, ":/settings.html", "text/html; charset=utf-8");
    } else if (host == "version" || host == "about") {
        serveResource(job, ":/about.html", "text/html; charset=utf-8");
    } else if (host == "flags") {
        serveResource(job, ":/flags.html", "text/html; charset=utf-8");
    } else if (host == "bookmarks") {
        serveResource(job, ":/bookmarks.html", "text/html; charset=utf-8");
    } else if (host == "history") {
        serveResource(job, ":/history.html", "text/html; charset=utf-8");
    } else if (host == "downloads") {
        serveResource(job, ":/downloads.html", "text/html; charset=utf-8");
    } else if (host == "extensions") {
        serveResource(job, ":/extensions.html", "text/html; charset=utf-8");
    } else if (host == "permissions") {
        serveResource(job, ":/permissions.html", "text/html; charset=utf-8");
    } else if (host == "files") {
        // A local directory listing. The path being browsed arrives as a query
        // parameter rather than as the URL's path: a file system path contains
        // characters (``#``, ``?``, spaces) that a URL path would mangle, and the
        // page reads ``location.search`` to know where it is.
        serveResource(job, ":/files.html", "text/html; charset=utf-8");
    } else {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
    }
}
