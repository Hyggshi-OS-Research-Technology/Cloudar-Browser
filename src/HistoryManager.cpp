#include "HistoryManager.h"

#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>

HistoryManager::HistoryManager(QObject *parent) : QObject(parent)
{
    load();
}

QString HistoryManager::storagePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/history.tsv";
}

void HistoryManager::addVisit(const QString &title, const QString &url)
{
    HistoryEntry entry{title, url, QDateTime::currentDateTime()};
    m_entries.prepend(entry);

    // Cap in-memory history to keep memory footprint predictable.
    constexpr int kMaxEntries = 5000;
    if (m_entries.size() > kMaxEntries)
        m_entries.resize(kMaxEntries);

    save();
}

void HistoryManager::clear()
{
    m_entries.clear();
    save();
}

void HistoryManager::load()
{
    QFile file(storagePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QStringList parts = line.split('\t');
        if (parts.size() != 3)
            continue;
        HistoryEntry entry;
        entry.visitedAt = QDateTime::fromString(parts[0], Qt::ISODate);
        entry.title = parts[1];
        entry.url = parts[2];
        m_entries.append(entry);
    }
}

void HistoryManager::save() const
{
    QFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    for (const auto &e : m_entries)
        out << e.visitedAt.toString(Qt::ISODate) << '\t' << e.title << '\t' << e.url << '\n';
}
