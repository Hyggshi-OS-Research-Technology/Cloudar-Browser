#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVector>

struct HistoryEntry {
    QString title;
    QString url;
    QDateTime visitedAt;
};

// Simple flat-file (JSON lines) history store — avoids pulling in a full
// SQL driver just to remember visited pages, which keeps the binary lean.
class HistoryManager : public QObject
{
    Q_OBJECT
public:
    explicit HistoryManager(QObject *parent = nullptr);

    void addVisit(const QString &title, const QString &url);
    QVector<HistoryEntry> entries() const { return m_entries; }
    void clear();

signals:
    // Emitted after any change, so cloudar://history can refresh itself. The
    // manager is the only writer, so the page never has to poll.
    void changed();

private:
    QString storagePath() const;
    void load();
    void save() const;

    QVector<HistoryEntry> m_entries;
};
