#include "LightSchemeHandler.h"

#include "FaviconStore.h"

#include <QWebEngineUrlScheme>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QUrlQuery>

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

void LightSchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
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
    } else {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
    }
}
