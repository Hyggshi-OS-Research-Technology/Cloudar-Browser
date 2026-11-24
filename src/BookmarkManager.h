#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QStringList>

struct BookmarkEntry {
    QString title;
    QString url;
    // Optional folder name, "" = unfiled. cloudar://bookmarks groups by this
    // and lets the user create/delete folders; the bookmarks bar ignores it
    // and keeps showing every bookmark in file order, which is what the
    // toolbar has always done.
    QString folder;
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

    void add(const QString &title, const QString &url, const QString &folder = QString());
    void addFolder(const QString &name);
    // `withBookmarks` decides whether the folder's bookmarks are deleted along
    // with it or moved back to "unfiled". The page asks the user which.
    void removeFolder(const QString &name, bool withBookmarks);
    QStringList folders() const;
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
    QStringList m_folders;
    QString storagePath() const;
    void load();
    void save() const;

    QVector<BookmarkEntry> m_entries;
};
