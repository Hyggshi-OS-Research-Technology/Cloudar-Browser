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

    // lastContextMenuRequest() lives on the *view* in Qt 6, not on the page.
    QWebEngineContextMenuRequest *request = m_view->lastContextMenuRequest();
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
            m_view->page()->triggerAction(QWebEnginePage::OpenLinkInThisWindow);
        } else if (chosen == openNewTab) {
            m_view->page()->triggerAction(QWebEnginePage::OpenLinkInNewTab);
        } else if (chosen == openNewWindow) {
            // No engine action opens a separate *window*, only a tab, so this is
            // the one link item that goes through MainWindow.
            emit openInNewWindow(link);
        } else if (chosen == copyLink) {
            m_view->page()->triggerAction(QWebEnginePage::CopyLinkToClipboard);
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
            // The engine knows whether this is image or video and which of the
            // two clipboard flavours applies; no need to guess from the URL.
            m_view->page()->triggerAction(onImage
                    ? QWebEnginePage::CopyImageUrlToClipboard
                    : QWebEnginePage::CopyMediaUrlToClipboard);
        } else if (chosen == saveMedia) {
            // Qt 6 has no DownloadAttributePolicy overload: it is
            // download(QUrl, filename), and an empty filename lets the profile's
            // download handler (DownloadManager) choose one.
            m_view->page()->download(media, QString());
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
    inspect->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    QAction *devTools = menu.addAction(tr("Developer tools"));
    devTools->setShortcut(QKeySequence(Qt::Key_F12));

    QAction *chosen = menu.exec(m_view->mapToGlobal(pos));
    if (!chosen)
        return;
    if (chosen == back) m_view->back();
    else if (chosen == forward) m_view->forward();
    else if (chosen == reload) m_view->reload();
    else if (chosen == copyPageUrl) QApplication::clipboard()->setText(pageUrl.toString());
    else if (chosen == pageSource && m_pageSourceHandler) m_pageSourceHandler(pageUrl);
    else if (chosen == printPage) emit printRequested();
    else if (chosen == inspect || chosen == devTools) {
        // QtWebEngineWidgets has no in-page inspector overlay: DevTools lives in
        // its own QWebEngineView showing devtools://devtools/bundled/inspector.html
        // with setInspectedPage() pointing at this page. triggerAction(
        // InspectElement) is what opens it, but the view that appears is created
        // by Qt with no chrome of its own — which is why MainWindow builds a
        // proper window for it instead (see MainWindow::openDevTools).
        emit devToolsRequested();
    }
}
