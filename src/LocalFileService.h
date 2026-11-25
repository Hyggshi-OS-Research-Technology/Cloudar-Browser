#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QUrl>

// ---------------------------------------------------------------------------
// Local file access for cloudar://files/ and the address bar.
//
// Why this is a class of its own rather than a few helpers on InternalBridge:
// everything that touches the filesystem on behalf of a *web page* has to obey
// one rule, and that rule needs somewhere to live.
//
// The rule: a cloudar:// page is treated by Chromium as local content, and
// LocalContentCanAccessRemoteUrls is on so the New Tab wallpaper can load a
// remote image. A page that can read arbitrary directories through a bridge
// would be a much bigger hole than that, so directory listing is **path
// confined**: the page can only ever see inside a root the *user* opened
// (Ctrl+O, the address bar, or a click in the listing itself). Every path that
// arrives from the page is canonicalised and checked against that root, which
// also means ".." and symlinks cannot climb out of it.
//
// The listing itself is data only — names, sizes, dates, and URLs. Nothing here
// ever returns file *contents*; a click on a file navigates the tab to a
// file:// URL, which is exactly what typing the path would have done.
// ---------------------------------------------------------------------------
class LocalFileService : public QObject
{
    Q_OBJECT
public:
    explicit LocalFileService(QObject *parent = nullptr);

    // --- address bar -------------------------------------------------------
    // Resolves user input to a local path if (and only if) it really is one.
    // Returns an empty QUrl for anything that should stay a search query or a
    // normal URL. This is what replaces the old "does it contain a dot?"
    // heuristic: a directory has no dot in it, and "../notes" has no scheme.
    //
    // `*path` receives the canonical absolute path when the return value is a
    // local URL, so callers that want to remember the tab's root do not have to
    // parse the URL back.
    static QUrl resolveUserInput(const QString &input, QString *path = nullptr);

    // file:// URL for a path, percent-encoded correctly (a space or a '#' in a
    // filename must not change the URL's meaning).
    static QUrl urlForLocalPath(const QString &path);

    // --- roots -------------------------------------------------------------
    // Directories the user has opened. A path is listable if it is inside one of
    // them. Roots are per session, not persisted: reopening the browser should
    // not leave a page able to list last week's directory.
    void addRoot(const QString &directory);
    QStringList roots() const { return m_roots; }
    bool isUnderRoot(const QString &path) const;

    // --- listing -----------------------------------------------------------
    // { ok, root, path, parent, name, entries: [{name, path, url, isDir, size,
    //   modified, hidden}], error }
    // `directory` may be empty, in which case the most recently added root is
    // used — that is the "open the listing page for the folder I just opened"
    // case.
    QVariantMap listDirectory(const QString &directory);

signals:
    // Emitted when a root is added, so a listing page that is already open can
    // follow the user into the new folder instead of showing the empty state.
    void rootsChanged();

private:
    QStringList m_roots;
};
