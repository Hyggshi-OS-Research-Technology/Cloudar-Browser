#pragma once

#include <QFrame>

class BrowserTabWidget;
class QListWidget;
class QToolButton;
class QLabel;

// A real vertical tab surface. It replaces (rather than duplicates) the
// horizontal tab strip whenever the sidebar preference is enabled.
class BrowserSidebar : public QFrame
{
    Q_OBJECT
public:
    explicit BrowserSidebar(BrowserTabWidget *tabs, QWidget *parent = nullptr);
    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void hideRequested();
    void expandedChanged(bool expanded);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    BrowserTabWidget *m_tabs;
    QListWidget *m_tabList;
    QToolButton *m_toggleButton;
    QToolButton *m_newTabButton;
    QLabel *m_title;
    QWidget *m_hoverCard = nullptr;
    bool m_expanded = false;

    void refresh();
};
