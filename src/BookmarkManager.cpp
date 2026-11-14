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

void BookmarkManager::add(const QString &title, const QString &url)
{
    if (contains(url))
        return;
    m_entries.append(BookmarkEntry{title.isEmpty() ? url : title, url});
    save();
    emit changed();
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
        const QStringList parts = line.split('\t');
        if (parts.size() != 2)
            continue;
        m_entries.append(BookmarkEntry{parts[0], parts[1]});
    }
}

void BookmarkManager::save() const
{
    QFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    for (const auto &e : m_entries)
        out << e.title << '\t' << e.url << '\n';
}
