#pragma once

#include <QToolButton>
#include <QWidget>
#include <QUrl>

class QHBoxLayout;
class QLabel;
class BookmarkManager;

// A single bookmark's button in the bar. Handles its own drag-to-reorder
// (QDrag carrying its index as plain-text mime data) and a right-click
// context menu for rename/delete, so BookmarkBar itself only has to wire
// up signals rather than track mouse state for every button.
class BookmarkButton : public QToolButton
{
    Q_OBJECT
public:
    explicit BookmarkButton(int index, const QString &title, const QUrl &url, QWidget *parent = nullptr);

    int index() const { return m_index; }
    void setIndex(int index) { m_index = index; }
    QUrl url() const { return m_url; }

signals:
    void activated(const QUrl &url);
    void reorderRequested(int fromIndex, int toIndex);
    void removeRequested(int index);
    void renameRequested(int index, const QString &newTitle);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    int m_index;
    QUrl m_url;
    QPoint m_dragStartPos;
};

// Horizontal strip of bookmark buttons directly below the navigation
// toolbar (see MainWindow::buildBookmarksBar). Rebuilds itself from
// BookmarkManager on every change rather than diffing, since bookmark
// lists are small enough that this is cheap and keeps the code simple.
class BookmarkBar : public QWidget
{
    Q_OBJECT
public:
    explicit BookmarkBar(BookmarkManager *manager, QWidget *parent = nullptr);

    void refresh();

signals:
    void bookmarkActivated(const QUrl &url);

private:
    BookmarkManager *m_manager;
    QHBoxLayout *m_layout;
    QLabel *m_emptyHint;
};
