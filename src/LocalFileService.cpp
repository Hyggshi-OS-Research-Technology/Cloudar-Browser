#include "LocalFileService.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>


namespace {

// A directory with 50 000 files would turn into a 50 000-element QVariantList
// marshalled across QWebChannel before the page has drawn anything. The listing
// reports its own truncation rather than silently showing a partial list.
constexpr int kMaxEntries = 2000;

// `~` and `~/...` expand to the home directory. Not a shell: only the leading
// tilde is expanded, because "a~b/file" is a perfectly legal filename.
QString expandTilde(const QString &input)
{
    if (input == QLatin1String("~"))
        return QDir::homePath();
    if (input.startsWith(QLatin1String("~/")))
        return QDir::homePath() + input.mid(1);
    return input;
}

// Canonical where possible (this is what resolves symlinks, and therefore what
// makes the ".." check below meaningful), cleaned where the path does not exist
// yet. An unresolvable path is not trusted to be inside anything.
QString canonicalOrCleaned(const QString &path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    if (!canonical.isEmpty())
        return canonical;
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

// True when `path` is `root` itself or sits below it. Compares whole path
// components, so /home/user2 is not considered to be inside /home/user.
bool pathIsInside(const QString &path, const QString &root)
{
    if (root.isEmpty())
        return false;
    if (path == root)
        return true;
    return path.startsWith(root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/'));
}

// Inputs that are meant as paths. Deliberately narrow: a bare word is a search
// query even if a directory of that name happens to exist next to the working
// directory, because a launched GUI app's working directory is not something
// the user can see or predict.
bool looksLikeLocalPath(const QString &input)
{
    if (input.startsWith(QLatin1Char('/')))
        return true;                       // POSIX absolute
    // "~" alone is the home directory, which is a path and not a search for
    // the word "~". The test caught this: expandTilde() handled it, but the
    // path-vs-search gate rejected it before it ever got there.
    if (input == QLatin1String("~"))
        return true;
    if (input.startsWith(QLatin1String("~/")))
        return true;                       // home-relative
    if (input.startsWith(QLatin1String("\\\\")))
        return true;                       // Windows UNC
    // Windows drive-absolute: C:\ or C:/
    static const QRegularExpression drive(QStringLiteral("^[A-Za-z]:[\\\\/]"));
    return drive.match(input).hasMatch();
}

} // namespace

LocalFileService::LocalFileService(QObject *parent) : QObject(parent) {}

QUrl LocalFileService::urlForLocalPath(const QString &path)
{
    if (path.isEmpty())
        return QUrl();
    // fromLocalFile() does the percent-encoding, including '#' and '?' in a
    // filename, which would otherwise be read back as a fragment/query.
    return QUrl::fromLocalFile(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
}

QUrl LocalFileService::resolveUserInput(const QString &input, QString *path)
{
    QString text = input.trimmed();
    if (text.isEmpty())
        return QUrl();

    // Windows "Copy as path" wraps the path in double quotes; Qt's own file
    // dialogs never do, but the clipboard is where these come from.
    if (text.size() >= 2 && text.startsWith(QLatin1Char('"')) && text.endsWith(QLatin1Char('"')))
        text = text.mid(1, text.size() - 2).trimmed();

    // An explicit file:// URL is already local; honour it even when the target
    // is missing so the page shows Chromium's own error rather than a search.
    if (text.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
        const QString local = QUrl(text).toLocalFile();
        if (local.isEmpty())
            return QUrl();
        if (path)
            *path = QDir::cleanPath(local);
        return urlForLocalPath(local);
    }

    if (!looksLikeLocalPath(text))
        return QUrl();

    const QString expanded = expandTilde(text);
    const QFileInfo info(expanded);
    if (!info.exists())
        return QUrl();

    const QString absolute = info.canonicalFilePath().isEmpty()
                                 ? QDir::cleanPath(info.absoluteFilePath())
                                 : info.canonicalFilePath();
    if (path)
        *path = absolute;
    return urlForLocalPath(absolute);
}

void LocalFileService::addRoot(const QString &directory)
{
    const QString canonical = canonicalOrCleaned(directory);
    if (canonical.isEmpty() || !QFileInfo(canonical).isDir())
        return;
    if (m_roots.contains(canonical))
        return;
    // Most recent first: listDirectory("") and the "I just opened a folder"
    // case both mean the one the user chose a moment ago.
    m_roots.prepend(canonical);
    emit rootsChanged();
}

bool LocalFileService::isUnderRoot(const QString &path) const
{
    const QString canonical = canonicalOrCleaned(path);
    if (canonical.isEmpty())
        return false;
    for (const QString &root : m_roots) {
        if (pathIsInside(canonical, canonicalOrCleaned(root)))
            return true;
    }
    return false;
}

QVariantMap LocalFileService::listDirectory(const QString &directory)
{
    QVariantMap payload;

    QString target = directory.trimmed();
    if (target.isEmpty()) {
        if (m_roots.isEmpty()) {
            payload[QStringLiteral("ok")] = false;
            // A code, not a sentence: the page formats it, the C++ side must not
            // decide which language the message is in (same rule as the hover
            // card's state label).
            payload[QStringLiteral("error")] = QStringLiteral("no-root");
            return payload;
        }
        target = m_roots.first();
    }

    // The confinement check. Everything below this line is only reachable for a
    // directory the user has opened.
    if (!isUnderRoot(target)) {
        payload[QStringLiteral("ok")] = false;
        payload[QStringLiteral("error")] = QStringLiteral("outside-root");
        return payload;
    }

    const QFileInfo dirInfo(target);
    if (!dirInfo.exists() || !dirInfo.isDir()) {
        payload[QStringLiteral("ok")] = false;
        payload[QStringLiteral("error")] = QStringLiteral("not-a-directory");
        return payload;
    }

    const QString canonicalDir = dirInfo.canonicalFilePath();
    QDir dir(canonicalDir);
    if (!dir.isReadable()) {
        payload[QStringLiteral("ok")] = false;
        payload[QStringLiteral("error")] = QStringLiteral("unreadable");
        payload[QStringLiteral("path")] = canonicalDir;
        return payload;
    }

    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDir::DirsFirst | QDir::IgnoreCase | QDir::Name);

    QVariantList list;
    bool truncated = false;
    for (const QFileInfo &entry : entries) {
        if (list.size() >= kMaxEntries) {
            truncated = true;
            break;
        }
        // A broken symlink, a socket, a device node: they exist, so they are
        // listed, but a click must not navigate into something that is not a
        // file or a directory.
        const bool isDir = entry.isDir();
        const bool navigable = isDir || entry.isFile();

        QVariantMap item;
        item[QStringLiteral("name")] = entry.fileName();
        item[QStringLiteral("path")] = entry.absoluteFilePath();
        // The URL the page navigates to, built here so percent-encoding is done
        // by QUrl rather than by hand in JavaScript ('#', '?' and '%' in a
        // filename all break a hand-built file:// string).
        item[QStringLiteral("url")] = urlForLocalPath(entry.absoluteFilePath()).toString();
        item[QStringLiteral("isDir")] = isDir;
        item[QStringLiteral("navigable")] = navigable;
        item[QStringLiteral("size")] = isDir ? 0 : static_cast<qint64>(entry.size());
        item[QStringLiteral("modified")] = entry.lastModified().toString(Qt::ISODate);
        // Dotfiles on POSIX, the hidden attribute on Windows: Qt answers both.
        item[QStringLiteral("hidden")] = entry.isHidden();
        list.append(item);
    }

    // Where "up" goes: the parent, but only while it is still inside a root.
    // Otherwise the top of the tree would offer a way out of the confinement.
    QString parent;
    const QString parentPath = QFileInfo(canonicalDir).absolutePath();
    if (!parentPath.isEmpty() && parentPath != canonicalDir && isUnderRoot(parentPath))
        parent = parentPath;

    payload[QStringLiteral("ok")] = true;
    payload[QStringLiteral("path")] = canonicalDir;
    payload[QStringLiteral("name")] = QFileInfo(canonicalDir).fileName().isEmpty()
                                          ? canonicalDir
                                          : QFileInfo(canonicalDir).fileName();
    payload[QStringLiteral("parent")] = parent;
    payload[QStringLiteral("entries")] = list;
    payload[QStringLiteral("truncated")] = truncated;
    payload[QStringLiteral("entryLimit")] = kMaxEntries;
    return payload;
}
