#pragma once

#include <QObject>
#include <QString>
#include <QVector>

struct BookmarkEntry {
    QString title;
    QString url;
};

// Simple flat-file (TSV) bookmark store, mirroring HistoryManager's pattern
// so it stays consistent with the rest of the app instead of pulling in a
// SQL driver. Row order in the file *is* the display order in the
// bookmarks bar, so move() persists reordering directly — no separate
// "position" field needed.
class BookmarkManager : public QObject
{
    Q_OBJECT
public:
    explicit BookmarkManager(QObject *parent = nullptr);

    void add(const QString &title, const QString &url);
    void removeAt(int index);
    void rename(int index, const QString &newTitle);
    void move(int fromIndex, int toIndex);
    bool contains(const QString &url) const;
    int indexOfUrl(const QString &url) const;

    QVector<BookmarkEntry> entries() const { return m_entries; }

signals:
    // Emitted after any modification, so BookmarkBar (and the star action
    // in the nav toolbar) can refresh without MainWindow having to call
    // both explicitly every time.
    void changed();

private:
    QString storagePath() const;
    void load();
    void save() const;

    QVector<BookmarkEntry> m_entries;
};
