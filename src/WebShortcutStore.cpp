#include "WebShortcutStore.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>

#include <algorithm>

namespace {
const char *kSettingsKey = "browser/webShortcuts";

// A shortcut's id is generated once and never shown to the user; it exists so
// --app=<id> and a pinned launcher can refer to the entry even when its name is
// changed. The full UUID is fine: these are stored in QSettings, not in a URL
// the user types (the --app form takes the id as an argument, not a path).
QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// Compared without the trailing slash and without a fragment: "example.com/app"
// and "example.com/app/" are the same shortcut, "example.com/app#top" too.
QString normalized(const QUrl &url)
{
    QUrl copy = url;
    copy.setFragment(QString());
    QString text = copy.toString(QUrl::RemoveTrailingSlash);
    return text;
}
} // namespace

WebShortcutStore::WebShortcutStore(QObject *parent) : QObject(parent) {}

bool WebShortcutStore::isShortcutableUrl(const QUrl &url)
{
    // http/https only: a shortcut to cloudar://settings or file:///etc/passwd
    // would be a chrome-less window the user cannot navigate out of, and a
    // file:// one would be a second route to local files that bypasses the
    // confinement described in docs/FILE_PROTOCOL_SECURITY.md.
    return url.isValid() && !url.host().isEmpty()
           && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"));
}

QList<WebShortcutStore::Shortcut> WebShortcutStore::readAll() const
{
    QList<Shortcut> list;
    const QString raw = QSettings().value(kSettingsKey).toString();
    if (raw.isEmpty())
        return list;
    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    if (!doc.isArray())
        return list; // corrupt value: report "no shortcuts" rather than crash
    for (const QJsonValue &value : doc.array()) {
        const QJsonObject o = value.toObject();
        Shortcut s;
        s.id = o.value(QStringLiteral("id")).toString();
        s.name = o.value(QStringLiteral("name")).toString();
        s.url = QUrl(o.value(QStringLiteral("url")).toString());
        s.added = o.value(QStringLiteral("added")).toString();
        if (s.isValid())
            list.append(s);
    }
    return list;
}

void WebShortcutStore::writeAll(const QList<Shortcut> &shortcuts)
{
    QJsonArray array;
    for (const Shortcut &s : shortcuts) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), s.id);
        o.insert(QStringLiteral("name"), s.name);
        o.insert(QStringLiteral("url"), s.url.toString());
        o.insert(QStringLiteral("added"), s.added);
        array.append(o);
    }
    QSettings().setValue(kSettingsKey,
                         QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
    emit changed();
}

QList<WebShortcutStore::Shortcut> WebShortcutStore::all() const
{
    return readAll();
}

WebShortcutStore::Shortcut WebShortcutStore::find(const QString &id) const
{
    for (const Shortcut &s : readAll()) {
        if (s.id == id)
            return s;
    }
    return Shortcut();
}

WebShortcutStore::Shortcut WebShortcutStore::findByUrl(const QUrl &url) const
{
    const QString wanted = normalized(url);
    for (const Shortcut &s : readAll()) {
        if (normalized(s.url) == wanted)
            return s;
    }
    return Shortcut();
}

WebShortcutStore::Shortcut WebShortcutStore::add(const QString &name, const QUrl &url)
{
    if (!isShortcutableUrl(url))
        return Shortcut();

    QList<Shortcut> list = readAll();
    const QString wanted = normalized(url);
    for (Shortcut &s : list) {
        if (normalized(s.url) != wanted)
            continue;
        // Same site, possibly a new name: update rather than duplicate, and keep
        // the id so anything pinned to it still resolves.
        if (!name.trimmed().isEmpty())
            s.name = name.trimmed();
        writeAll(list);
        return s;
    }

    Shortcut created;
    created.id = newId();
    created.name = name.trimmed().isEmpty() ? url.host() : name.trimmed();
    created.url = url;
    created.added = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    list.append(created);
    writeAll(list);
    return created;
}

bool WebShortcutStore::remove(const QString &id)
{
    QList<Shortcut> list = readAll();
    const int before = list.size();
    // std::remove_if rather than QList::removeIf(): the latter is Qt 6.1+, and
    // this file has no other reason to need a version guard.
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&id](const Shortcut &s) { return s.id == id; }),
               list.end());
    if (list.size() == before)
        return false;
    writeAll(list);
    return true;
}
