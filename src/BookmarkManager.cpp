#include "BookmarkManager.h"

#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>

BookmarkManager::BookmarkManager(QObject *parent) : QObject(parent)
{
    load();
}

QString BookmarkManager::storagePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/bookmarks.tsv";
}

void BookmarkManager::add(const QString &title, const QString &url, const QString &folder)
{
    if (contains(url))
        return;
    const QString cleanFolder = folder.trimmed();
    if (!cleanFolder.isEmpty() && !m_folders.contains(cleanFolder))
        m_folders.append(cleanFolder);
    m_entries.append(BookmarkEntry{title.isEmpty() ? url : title, url, cleanFolder});
    save();
    emit changed();
}

void BookmarkManager::addFolder(const QString &name)
{
    const QString clean = name.trimmed();
    // An empty name would be indistinguishable from "unfiled".
    if (clean.isEmpty() || m_folders.contains(clean))
        return;
    m_folders.append(clean);
    save();
    emit changed();
}

void BookmarkManager::removeFolder(const QString &name, bool withBookmarks)
{
    if (!m_folders.removeOne(name))
        return;
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (m_entries[i].folder != name)
            continue;
        if (withBookmarks)
            m_entries.remove(i);
        else
            m_entries[i].folder.clear();
    }
    save();
    emit changed();
}

QStringList BookmarkManager::folders() const
{
    return m_folders;
}

void BookmarkManager::removeAt(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;
    m_entries.remove(index);
    save();
    emit changed();
}

void BookmarkManager::rename(int index, const QString &newTitle)
{
    if (index < 0 || index >= m_entries.size() || newTitle.isEmpty())
        return;
    m_entries[index].title = newTitle;
    save();
    emit changed();
}

void BookmarkManager::move(int fromIndex, int toIndex)
{
    if (fromIndex == toIndex || fromIndex < 0 || fromIndex >= m_entries.size() ||
        toIndex < 0 || toIndex >= m_entries.size())
        return;
    m_entries.move(fromIndex, toIndex);
    save();
    emit changed();
}

bool BookmarkManager::contains(const QString &url) const
{
    return indexOfUrl(url) != -1;
}

int BookmarkManager::indexOfUrl(const QString &url) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].url == url)
            return i;
    return -1;
}

void BookmarkManager::load()
{
    QFile file(storagePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        // Folders are stored as their own "FOLDER<TAB>name" rows rather than a
        // header block: keeping one line per record means an empty folder
        // survives a save/load cycle, and the two-column bookmark rows written
        // by earlier versions still parse (they simply have no folder).
        const QStringList parts = line.split('\t');
        if (parts.size() == 2 && parts[0] == QLatin1String("FOLDER")) {
            if (!parts[1].isEmpty() && !m_folders.contains(parts[1]))
                m_folders.append(parts[1]);
        } else if (parts.size() == 2) {
            m_entries.append(BookmarkEntry{parts[0], parts[1], QString()});
        } else if (parts.size() == 3) {
            if (!parts[2].isEmpty() && !m_folders.contains(parts[2]))
                m_folders.append(parts[2]);
            m_entries.append(BookmarkEntry{parts[0], parts[1], parts[2]});
        }
    }
}

void BookmarkManager::save() const
{
    QFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    for (const QString &folder : m_folders)
        out << "FOLDER\t" << folder << '\n';
    for (const auto &e : m_entries)
        out << e.title << '\t' << e.url << '\t' << e.folder << '\n';
}
