#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

// ---------------------------------------------------------------------------
// "Web shortcuts" — a saved site that opens in its own small window.
//
// This is the roadmap item called "Web shortcut", which is the closest thing
// this build can honestly offer to installing a PWA. Being clear about the
// difference matters, because the two look alike from a menu:
//
//   What this does: remembers a URL + a name, and opens it in a window whose
//   chrome is just a back/forward/reload row — no tab strip, no omnibox, no
//   bookmark bar, and a title bar that says the shortcut's name.
//
//   What it does NOT do: it does not install anything. There is no
//   manifest.json fetch, no service worker registration, no offline cache, no
//   icon extracted from the site's manifest, no Start-menu or desktop entry.
//   Close the browser and there is no cache to serve the app from, so a
//   shortcut that needs the network will not open without it. QtWebEngine has
//   no API that would change that (see docs/KNOWN_ISSUES.md §13).
//
// Storage is QSettings (a JSON array under "browser/webShortcuts"), not the
// renderer's storage partition, for the same reason the New Tab wallpaper is:
// the partition is cleared with browsing data, and a shortcut the user created
// deliberately should survive that.
// ---------------------------------------------------------------------------
class WebShortcutStore : public QObject
{
    Q_OBJECT
public:
    struct Shortcut {
        QString id;     // stable, generated once; used by `--app=<id>`
        QString name;   // what the title bar shows
        QUrl url;
        QString added;  // ISO date, informational only

        bool isValid() const { return !id.isEmpty() && url.isValid(); }
    };

    explicit WebShortcutStore(QObject *parent = nullptr);

    QList<Shortcut> all() const;
    // Empty id means "not stored".
    Shortcut find(const QString &id) const;
    // Matches on the URL, ignoring a trailing slash: two shortcuts to the same
    // site under different names would be a menu the user cannot tell apart.
    Shortcut findByUrl(const QUrl &url) const;

    // Adds a shortcut and returns it. An existing shortcut for the same URL is
    // updated in place (name only) rather than duplicated, and its id is kept so
    // a pinned launcher keeps working.
    Shortcut add(const QString &name, const QUrl &url);
    bool remove(const QString &id);

    // "id" for a URL that is not http(s) or is malformed. Callers use this to
    // refuse before showing a dialog.
    static bool isShortcutableUrl(const QUrl &url);

signals:
    void changed();

private:
    QList<Shortcut> readAll() const;
    void writeAll(const QList<Shortcut> &shortcuts);
};
