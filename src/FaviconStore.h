#pragma once

#include <QByteArray>
#include <QHash>
#include <QIcon>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

// ---------------------------------------------------------------------------
// Real favicons for internal pages.
//
// The New Tab speed dial used to show the first letter of each site name in a
// coloured square, because there was no way for a cloudar:// page to ask the
// browser "what icon does github.com actually use?". Chromium knows: it already
// downloads and decodes a favicon for every page it loads, and QWebEngineView
// exposes it through iconChanged(). This store keeps the largest pixmap of that
// icon on disk, keyed by host, and hands the path back out as a URL the page can
// put in an <img>:
//
//     cloudar://newtab/favicon?u=https%3A%2F%2Fgithub.com%2F
//
// Serving it through the scheme handler (rather than handing the page a base64
// blob or a file:// path) keeps the page free of native knowledge, works for any
// number of tiles, and is the only variant that also works under a CSP that
// blocks inline data: URLs.
//
// Persistence matters: a speed dial is worthless if every icon disappears when
// the browser restarts, so the PNGs live in <data>/favicons/ and are looked up
// lazily. A missing file simply means "never visited", and the page falls back
// to https://host/favicon.ico and then to a letter tile.
// ---------------------------------------------------------------------------
class FaviconStore : public QObject
{
    Q_OBJECT
public:
    explicit FaviconStore(QObject *parent = nullptr);

    // Called from BrowserTabWidget::wireView() on every iconChanged(). Does
    // nothing when the icon is null (Chromium sends an empty icon for pages
    // that have none, and overwriting a good icon with a blank one would make
    // the tile flicker back to a letter on the next reload).
    void record(const QUrl &pageUrl, const QIcon &icon);

    // "cloudar://favicon/?u=<page url>" when this host has a stored icon,
    // otherwise an empty string.
    QString cachedUrl(const QUrl &pageUrl) const;

    // Same as cachedUrl(), but on a miss it also starts downloading
    // https://<host>/favicon.ico in the background and emits faviconReady()
    // once it is stored. A speed dial entry the user added by hand was usually
    // never opened in this profile, so the cache alone would leave those tiles
    // as letters forever.
    QString faviconFor(const QUrl &pageUrl);

    // --- used by LightSchemeHandler --------------------------------------
    // Resolves a `?u=` value (a page URL) to the stored PNG bytes + MIME type.
    // Returns false when nothing is stored for that host.
    bool bytesForPageUrl(const QUrl &pageUrl, QByteArray *data, QByteArray *mimeType) const;

signals:
    // pageUrl is the URL that was asked about (not just the host), so a page
    // with several tiles on one host can match the reply to the right tile.
    void faviconReady(const QString &pageUrl, const QString &iconUrl);

private:
    void startFetch(const QString &host, const QString &pageUrl);
    void storePng(const QString &host, const QByteArray &png);
    QString directory() const;
    // "" if no icon is stored for this host.
    QString fileForHost(const QString &host) const;
    static QString hostOf(const QUrl &pageUrl);

    // host -> absolute file path, or "" once we know there is nothing stored.
    // Avoids a stat() per tile per New Tab load.
    mutable QHash<QString, QString> m_lookup;
    // Hosts we already have a request in flight for. Prevents one New Tab full
    // of tiles from firing the same download five times, and stops a site that
    // answers 404 from being retried on every keystroke of a reload loop.
    QSet<QString> m_pending;
    QSet<QString> m_failed;
    QNetworkAccessManager *m_network = nullptr;
};
