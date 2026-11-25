#pragma once

#include <QFrame>

class BrowserTabWidget;
class QListWidget;
class QToolButton;
class QLabel;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;

// A real vertical tab surface. It replaces (rather than duplicates) the
// horizontal tab strip whenever the sidebar preference is enabled.
class BrowserSidebar : public QFrame
{
    Q_OBJECT
public:
    explicit BrowserSidebar(BrowserTabWidget *tabs, QWidget *parent = nullptr);
    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded);
    // Re-reads the translated strings after a language change.
    void retranslate();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // --- Row drag and drop ---------------------------------------------------
    // A row widget calls this once the pointer has travelled past
    // QApplication::startDragDistance() with the left button still down. The
    // gesture runs, and finishes, entirely inside this method: QDrag::exec()
    // spins its own event loop, so everything after it is the "drop happened"
    // half. Drops back onto the list are reorders; a release outside this
    // window's frame tears the tab off into a new window.
    void beginTabDrag(int index, QWidget *rowWidget);
    // A row reports the press half of the gesture, because the rebuilds happen
    // between press and the first move that would start a drag: activating a tab
    // on press emits currentTabChanged, and if that rebuilt the list the row
    // under the cursor would be destroyed before it could ever become a drag.
    // While this is set, refresh() waits.
    void setRowPressActive(bool active) { m_rowPressActive = active; }
    // Runs the rebuild that was postponed while a row was pressed, if any.
    void flushDeferredRefresh();
    // Reorders the real tab order (QTabWidget's), which the row list mirrors.
    void moveTab(int from, int to);

signals:
    void hideRequested();
    void expandedChanged(bool expanded);
    // The user released a dragged tab outside the window. MainWindow owns the
    // window-creation policy, this widget only reports the gesture.
    void tabTearOffRequested(int index);

protected:
    void resizeEvent(QResizeEvent *event) override;
    // The sidebar, not the QListWidget, is the drop target: the rows mirror
    // QTabWidget's tab order, and a view that accepts drops would insert model
    // rows instead of moving tabs. See beginTabDrag() for the whole gesture.
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    BrowserTabWidget *m_tabs;
    QListWidget *m_tabList;
    QToolButton *m_toggleButton;
    QToolButton *m_newTabButton;
    QLabel *m_title;
    QWidget *m_hoverCard = nullptr;
    bool m_expanded = false;
    bool m_dragActive = false;
    bool m_rowPressActive = false;
    // Set when something asked for a rebuild while a drag was in flight; the
    // rebuild happens once the drag is over instead of under the drag's feet.
    bool m_refreshDeferred = false;
    // True when the gesture ended with a drop this widget actually handled, so
    // beginTabDrag() knows not to also treat it as a tear-off.
    bool m_droppedInList = false;
    // Thin line drawn between rows while a tab hovers over them. A plain child
    // widget rather than painting on the list: the rows are item widgets, and
    // anything drawn by the view's own paintEvent would be underneath them.
    QWidget *m_dropIndicator = nullptr;
    // Gap the line is currently in: 0 above the first row, count() below the
    // last. -1 while nothing is being dragged.
    int m_dropGap = -1;

    void refresh();
    void updateDropIndicator(const QPoint &sidebarPos);
    void clearDropIndicator();
};
