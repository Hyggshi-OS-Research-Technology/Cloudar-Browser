#include "BookmarkBar.h"
#include "BookmarkManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QInputDialog>
#include <QApplication>
#include <QLineEdit>

static const char *kBookmarkMimeType = "application/x-lightbrowser-bookmark-index";

BookmarkButton::BookmarkButton(int index, const QString &title, const QUrl &url, QWidget *parent)
    : QToolButton(parent), m_index(index), m_url(url)
{
    setText(title.isEmpty() ? url.toString() : title);
    setToolTip(url.toString());
    setAutoRaise(true);
    setCursor(Qt::PointingHandCursor);
    setToolButtonStyle(Qt::ToolButtonTextOnly);
    setMaximumWidth(160);
    setAcceptDrops(true);

    connect(this, &QToolButton::clicked, this, [this]() { emit activated(m_url); });
}

void BookmarkButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragStartPos = event->pos();
    QToolButton::mousePressEvent(event);
}

void BookmarkButton::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QToolButton::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
        QToolButton::mouseMoveEvent(event);
        return;
    }

    // Start a drag instead of letting this turn into a click — reordering
    // takes priority over activation once the pointer has moved far enough.
    auto *drag = new QDrag(this);
    auto *mime = new QMimeData();
    mime->setData(kBookmarkMimeType, QByteArray::number(m_index));
    drag->setMimeData(mime);
    drag->setPixmap(grab());
    drag->exec(Qt::MoveAction);
}

void BookmarkButton::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(kBookmarkMimeType))
        event->acceptProposedAction();
}

void BookmarkButton::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasFormat(kBookmarkMimeType))
        return;
    bool ok = false;
    int fromIndex = event->mimeData()->data(kBookmarkMimeType).toInt(&ok);
    if (ok && fromIndex != m_index)
        emit reorderRequested(fromIndex, m_index);
    event->acceptProposedAction();
}

void BookmarkButton::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    auto *openAction = menu.addAction("Open");
    auto *renameAction = menu.addAction("Rename");
    auto *deleteAction = menu.addAction("Delete");

    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == openAction) {
        emit activated(m_url);
    } else if (chosen == renameAction) {
        bool ok = false;
        QString newTitle = QInputDialog::getText(this, "Rename bookmark", "Name:", QLineEdit::Normal, text(), &ok);
        if (ok && !newTitle.isEmpty())
            emit renameRequested(m_index, newTitle);
    } else if (chosen == deleteAction) {
        emit removeRequested(m_index);
    }
}

BookmarkBar::BookmarkBar(BookmarkManager *manager, QWidget *parent)
    : QWidget(parent), m_manager(manager)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(6, 2, 6, 2);
    m_layout->setSpacing(4);

    m_emptyHint = new QLabel("Drag pages here or use the \u2606 button to add bookmarks", this);
    m_emptyHint->setStyleSheet("color: rgba(255,255,255,0.35); font-size: 11px;");

    connect(m_manager, &BookmarkManager::changed, this, &BookmarkBar::refresh);
    refresh();
}

void BookmarkBar::refresh()
{
    // Clear everything except the layout itself, then rebuild — bookmark
    // lists are small (dozens, not thousands), so a full rebuild on every
    // change is simpler and cheap enough versus diffing button widgets.
    QLayoutItem *item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        if (item->widget() && item->widget() != m_emptyHint)
            item->widget()->deleteLater();
        delete item;
    }

    const auto entries = m_manager->entries();
    if (entries.isEmpty()) {
        m_emptyHint->setVisible(true);
        m_layout->addWidget(m_emptyHint);
    } else {
        m_emptyHint->setVisible(false);
    }

    for (int i = 0; i < entries.size(); ++i) {
        auto *btn = new BookmarkButton(i, entries[i].title, QUrl(entries[i].url), this);
        btn->setAcceptDrops(true);

        connect(btn, &BookmarkButton::activated, this, &BookmarkBar::bookmarkActivated);
        connect(btn, &BookmarkButton::removeRequested, this, [this](int idx) { m_manager->removeAt(idx); });
        connect(btn, &BookmarkButton::renameRequested, this,
                [this](int idx, const QString &title) { m_manager->rename(idx, title); });
        connect(btn, &BookmarkButton::reorderRequested, this,
                [this](int from, int to) { m_manager->move(from, to); });

        m_layout->addWidget(btn);
    }

    m_layout->addStretch(1);
}
