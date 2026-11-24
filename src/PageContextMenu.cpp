#include "PageContextMenu.h"

#include "ClassicTheme.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMenu>
#include <QWebEngineContextMenuRequest>
#include <QWebEnginePage>
#include <QWebEngineView>

PageContextMenu::PageContextMenu(QWebEngineView *view, QObject *parent)
    : QObject(parent), m_view(view)
{
    if (!m_view)
        return;
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested,
            this, &PageContextMenu::showAt);
}

void PageContextMenu::showAt(const QPoint &pos)
{
    if (!m_view || !m_view->page())
        return;

    QWebEngineContextMenuRequest *request = m_view->page()->lastContextMenuRequest();
    QMenu menu(m_view);

    // Icons are drawn by ClassicTheme::icon() in whichever chrome is active, so
    // the same menu is monochrome on the 2009 chrome and light-on-dark on the
    // modern one instead of being hard-coded for one of them.
    const auto icon = [](const QString &name) { return Theme::icon(name, 16); };

    const bool onLink = request && !request->linkUrl().isEmpty();
    const bool onImage = request && !request->mediaUrl().isEmpty()
            && request->mediaType() == QWebEngineContextMenuRequest::MediaTypeImage;
    const bool onVideo = request && !request->mediaUrl().isEmpty()
            && request->mediaType() == QWebEngineContextMenuRequest::MediaTypeVideo;
    const bool onEditable = request && request->isContentEditable();
    const bool onSelection = request && !request->selectedText().isEmpty();
    const QUrl pageUrl = m_view->url();
    const bool internal = pageUrl.scheme() == QLatin1String("cloudar")
            || pageUrl.scheme() == QLatin1String("browser")
            || pageUrl.scheme() == QLatin1String("light");

    // --- link ------------------------------------------------------------
    if (onLink) {
        const QUrl link = request->linkUrl();
        QAction *openHere = menu.addAction(icon("page"), tr("Open link"));
        QAction *openNewTab = menu.addAction(icon("plus"), tr("Open link in new tab"));
        QAction *openNewWindow = menu.addAction(tr("Open link in new window"));
        QAction *copyLink = menu.addAction(tr("Copy link address"));
        menu.addSeparator();

        QAction *chosen = menu.exec(m_view->mapToGlobal(pos));
        if (!chosen)
            return;
        if (chosen == openHere) {
            m_view->setUrl(link);
        } else if (chosen == openNewTab) {
            emit openInNewTab(link);
        } else if (chosen == openNewWindow) {
            emit openInNewWindow(link);
        } else if (chosen == copyLink) {
            QApplication::clipboard()->setText(link.toString());
        }
        return;
    }

    // --- image / video ---------------------------------------------------
    if (onImage || onVideo) {
        const QUrl media = request->mediaUrl();
        QAction *openMedia = menu.addAction(tr("Open %1 in new tab")
                                            .arg(onImage ? tr("image") : tr("video")));
        QAction *copyMediaUrl = menu.addAction(tr("Copy %1 address")
                                               .arg(onImage ? tr("image") : tr("video")));
        QAction *saveMedia = menu.addAction(tr("Save %1 as…")
                                            .arg(onImage ? tr("image") : tr("video")));
        menu.addSeparator();
        QAction *chosen = menu.exec(m_view->mapToGlobal(pos));
        if (!chosen)
            return;
        if (chosen == openMedia) {
            emit openInNewTab(media);
        } else if (chosen == copyMediaUrl) {
            QApplication::clipboard()->setText(media.toString());
        } else if (chosen == saveMedia) {
            // A file dialog and QFile::copy would only work for file:// media;
            // for http(s) the page's own download machinery already handles it,
            // so trigger that rather than reimplementing it badly here.
            m_view->page()->download(media,
                                     QString(),
                                     QWebEnginePage::DownloadAttributePolicy::DownloadAllowed);
        }
        return;
    }

    // --- editable field --------------------------------------------------
    if (onEditable) {
        QAction *undo = menu.addAction(tr("Undo"));
        undo->setEnabled(m_view->page()->action(QWebEnginePage::Undo)->isEnabled());
        QAction *redo = menu.addAction(tr("Redo"));
        redo->setEnabled(m_view->page()->action(QWebEnginePage::Redo)->isEnabled());
        menu.addSeparator();
        QAction *cut = menu.addAction(icon("stop"), tr("Cut"));
        cut->setEnabled(m_view->page()->action(QWebEnginePage::Cut)->isEnabled());
        QAction *copy = menu.addAction(tr("Copy"));
        copy->setEnabled(m_view->page()->action(QWebEnginePage::Copy)->isEnabled());
        QAction *paste = menu.addAction(tr("Paste"));
        paste->setEnabled(m_view->page()->action(QWebEnginePage::Paste)->isEnabled());
        menu.addSeparator();
        QAction *selectAll = menu.addAction(tr("Select all"));
        QAction *chosen = menu.exec(m_view->mapToGlobal(pos));
        if (!chosen)
            return;
        if (chosen == undo) m_view->page()->triggerAction(QWebEnginePage::Undo);
        else if (chosen == redo) m_view->page()->triggerAction(QWebEnginePage::Redo);
        else if (chosen == cut) m_view->page()->triggerAction(QWebEnginePage::Cut);
        else if (chosen == copy) m_view->page()->triggerAction(QWebEnginePage::Copy);
        else if (chosen == paste) m_view->page()->triggerAction(QWebEnginePage::Paste);
        else if (chosen == selectAll) m_view->page()->triggerAction(QWebEnginePage::SelectAll);
        return;
    }

    // --- selected text ---------------------------------------------------
    if (onSelection) {
        QAction *copy = menu.addAction(tr("Copy"));
        QAction *search = menu.addAction(tr("Search the web for “%1”")
                                         .arg(request->selectedText().left(24)));
        menu.addSeparator();
        QAction *chosen = menu.exec(m_view->mapToGlobal(pos));
        if (!chosen)
            return;
        if (chosen == copy) {
            QApplication::clipboard()->setText(request->selectedText());
        } else if (chosen == search) {
            emit searchRequested(request->selectedText());
        }
        return;
    }

    // --- plain page ------------------------------------------------------
    QAction *back = menu.addAction(icon("back"), tr("Back"));
    back->setEnabled(m_view->page()->action(QWebEnginePage::Back)->isEnabled());
    QAction *forward = menu.addAction(icon("forward"), tr("Forward"));
    forward->setEnabled(m_view->page()->action(QWebEnginePage::Forward)->isEnabled());
    QAction *reload = menu.addAction(icon("reload"), tr("Reload"));
    menu.addSeparator();

    QAction *copyPageUrl = menu.addAction(tr("Copy page address"));
    QAction *pageSource = menu.addAction(tr("View page source"));
    // view-source: only makes sense for a real document. A cloudar:// page is a
    // Qt resource; "View page source" there would open a blank tab.
    pageSource->setEnabled(!internal && pageUrl.isValid());
    // "Print" in a browser means a print dialog with a preview; QtWebEngine only
    // offers printToPdf. The label says what the item actually does.
    QAction *printPage = menu.addAction(tr("Print to PDF…"));

    QAction *bookmarkAction = m_bookmarkActionProvider ? m_bookmarkActionProvider() : nullptr;
    if (bookmarkAction && bookmarkAction->isEnabled()) {
        menu.addSeparator();
        menu.addAction(bookmarkAction);
    }

    menu.addSeparator();
    QAction *inspect = menu.addAction(tr("Inspect element"));
    inspect->setCheckable(false);

    QAction *chosen = menu.exec(m_view->mapToGlobal(pos));
    if (!chosen)
        return;
    if (chosen == back) m_view->back();
    else if (chosen == forward) m_view->forward();
    else if (chosen == reload) m_view->reload();
    else if (chosen == copyPageUrl) QApplication::clipboard()->setText(pageUrl.toString());
    else if (chosen == pageSource && m_pageSourceHandler) m_pageSourceHandler(pageUrl);
    else if (chosen == printPage) emit printRequested();
    else if (chosen == inspect) {
        // QtWebEngineWidgets has no scripting inspector; the DevTools window is
        // the supported route and is what "Inspect" means to anyone using it.
        m_view->page()->triggerAction(QWebEnginePage::InspectElement);
    }
}
