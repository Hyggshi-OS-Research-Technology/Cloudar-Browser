#include "FaviconStore.h"

#include "BrowserResources.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>

namespace {

// Chromium hands us whatever size it decoded; picking the largest candidate
// gives the sharpest 34 px tile in the speed dial on HiDPI screens without
// having to rescale anything.
QPixmap largestPixmap(const QIcon &icon)
{
    QPixmap best;
    const QList<QSize> sizes = icon.availableSizes();
    for (const QSize &size : sizes) {
        const QPixmap candidate = icon.pixmap(size);
        if (candidate.isNull())
            continue;
        if (best.isNull() || candidate.width() > best.width())
            best = candidate;
    }
    if (best.isNull())
        best = icon.pixmap(64, 64);
    return best;
}

} // namespace

FaviconStore::FaviconStore(QObject *parent) : QObject(parent)
{
    m_network = new QNetworkAccessManager(this);
}

QString FaviconStore::directory() const
{
    const QString dirPath = BrowserResources::getBrowserDataDir() + QStringLiteral("/favicons");
    QDir dir(dirPath);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));
    return dirPath;
}

QString FaviconStore::hostOf(const QUrl &pageUrl)
{
    // A port is part of the origin but not of the icon; two ports on one host
    // almost always serve the same favicon, and collapsing them keeps the cache
    // from growing a file per development port.
    return pageUrl.host().toLower();
}

QString FaviconStore::fileForHost(const QString &host) const
{
    if (host.isEmpty())
        return QString();

    const auto cached = m_lookup.constFind(host);
    if (cached != m_lookup.constEnd())
        return *cached;

    // Hostnames only ever contain [a-z0-9.-] plus ':' for an explicit port and
    // punycode's "xn--", so percent-encoding is both a safe filename and
    // reversible by eye when debugging the cache directory.
    const QString name = QString::fromLatin1(QUrl::toPercentEncoding(host)) + QStringLiteral(".png");
    const QString path = directory() + QLatin1Char('/') + name;
    const QString resolved = QFile::exists(path) ? path : QString();
    m_lookup.insert(host, resolved);
    return resolved;
}

void FaviconStore::storePng(const QString &host, const QByteArray &png)
{
    if (host.isEmpty() || png.isEmpty())
        return;
    const QString path = directory() + QLatin1Char('/')
            + QString::fromLatin1(QUrl::toPercentEncoding(host)) + QStringLiteral(".png");
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    out.write(png);
    out.close();
    m_lookup.insert(host, path);
}

void FaviconStore::startFetch(const QString &host, const QString &pageUrl)
{
    if (host.isEmpty() || m_pending.contains(host) || m_failed.contains(host))
        return;
    m_pending.insert(host);

    // /favicon.ico at the root is the one location every browser has supported
    // since the 1990s, and it is what Chrome itself falls back to. Fetching the
    // page's <link rel=icon> would need a full HTML parse for a cosmetic gain.
    QNetworkRequest request(QUrl(QStringLiteral("https://") + host + QStringLiteral("/favicon.ico")));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    // Identify honestly: this is Cloudar fetching an icon for its New Tab page,
    // not a browser pretending to be Chrome in order to sneak past a block.
    request.setRawHeader("User-Agent", "Cloudar Browser (New Tab favicon)");

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, host, pageUrl]() {
        reply->deleteLater();
        m_pending.remove(host);

        if (reply->error() != QNetworkReply::NoError) {
            m_failed.insert(host);
            return;
        }
        const QByteArray raw = reply->readAll();
        // ICO is not in Qt's default image handlers, so decode through QImage
        // with autodetection; a site that serves PNG under /favicon.ico works
        // the same way, and the store always writes PNG regardless of input.
        QImage image;
        if (!image.loadFromData(raw) || image.isNull()) {
            m_failed.insert(host);
            return;
        }
        if (image.width() > 64 || image.height() > 64)
            image = image.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG")) {
            m_failed.insert(host);
            return;
        }
        buffer.close();
        storePng(host, png);
        emit faviconReady(pageUrl, cachedUrl(QUrl(pageUrl)));
    });
}

QString FaviconStore::faviconFor(const QUrl &pageUrl)
{
    const QString host = hostOf(pageUrl);
    if (host.isEmpty())
        return QString();
    const QString cached = cachedUrl(pageUrl);
    if (!cached.isEmpty())
        return cached;
    startFetch(host, pageUrl.toString(QUrl::RemoveFragment));
    return QString();
}

void FaviconStore::record(const QUrl &pageUrl, const QIcon &icon)
{
    const QString host = hostOf(pageUrl);
    if (host.isEmpty() || icon.isNull())
        return;
    // Internal pages have their own icon; storing it under "newtab" would put a
    // browser logo in the speed dial for a tile the user never added.
    if (pageUrl.scheme() != QLatin1String("https") && pageUrl.scheme() != QLatin1String("http"))
        return;

    const QPixmap pixmap = largestPixmap(icon);
    if (pixmap.isNull())
        return;

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!pixmap.save(&buffer, "PNG"))
        return;

    const QString path = directory() + QLatin1Char('/')
            + QString::fromLatin1(QUrl::toPercentEncoding(host)) + QStringLiteral(".png");

    // Rewriting an unchanged icon on every page load would churn the disk for
    // nothing; the size is a good enough fingerprint because Chromium re-decodes
    // the same asset to the same dimensions.
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly) && existing.size() == bytes.size())
        return;
    existing.close();

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    out.write(bytes);
    out.close();
    m_lookup.insert(host, path);
}

QString FaviconStore::cachedUrl(const QUrl &pageUrl) const
{
    const QString host = hostOf(pageUrl);
    if (host.isEmpty() || fileForHost(host).isEmpty())
        return QString();
    // Same origin as the New Tab page on purpose: a cross-host image on a custom
    // scheme is exactly the kind of request a future security policy would start
    // blocking, and there is no reason to depend on it.
    return QStringLiteral("cloudar://newtab/favicon?u=")
            + QString::fromUtf8(QUrl::toPercentEncoding(pageUrl.toString(QUrl::RemoveFragment)));
}

bool FaviconStore::bytesForPageUrl(const QUrl &pageUrl, QByteArray *data, QByteArray *mimeType) const
{
    const QString host = hostOf(pageUrl);
    if (host.isEmpty())
        return false;
    const QString path = fileForHost(host);
    if (path.isEmpty())
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    if (data)
        *data = file.readAll();
    if (mimeType)
        *mimeType = "image/png";
    return true;
}
