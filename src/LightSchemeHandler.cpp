#include "LightSchemeHandler.h"

#include <QWebEngineUrlScheme>
#include <QBuffer>
#include <QFile>

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

void LightSchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    // Hosts are matched case-insensitively, so cloudar://Settings/ and
    // cloudar://settings/ both resolve — the requested URL's casing and any
    // path/section after the host (e.g. cloudar://settings/privacy) are left
    // intact for the page itself; settings.html reads location.pathname to
    // deep-link to a section.
    const QString host = job->requestUrl().host().toLower();

    if (host == "newtab" || host == "new" || host.isEmpty()) {
        serveResource(job, ":/newtab.html", "text/html; charset=utf-8");
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
