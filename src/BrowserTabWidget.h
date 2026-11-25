#pragma once

#include <QTabWidget>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebChannel>
#include <QHash>
#include <QPixmap>
#include <functional>
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#include <QWebEnginePermission>
#endif

class QTimer;
class QToolButton;
class TabPreviewPopup;
class WebExtensionManager;
class FaviconStore;
class PageContextMenu;

// A QTabWidget where each tab hosts one QWebEngineView. All views share a
// single QWebEngineProfile so cookies/cache/history stay consistent and
// memory isn't duplicated per tab (Chromium's process-per-site-instance
// model still applies underneath, but we avoid extra profile overhead).
class BrowserTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    // `channel` carries the NativeBridge object exposed only to cloudar://
    // internal pages — see wireView(), which attaches/detaches it based on
    // the page's URL scheme so ordinary websites never see it.
    explicit BrowserTabWidget(QWebEngineProfile *profile, QWebChannel *channel,
                               QWidget *parent = nullptr);

    QWebEngineView *addNewTab(const QUrl &url = QUrl("cloudar://newtab"),
                               bool makeCurrent = true);
    QWebEngineView *currentView() const;
    // Painted, palette-following close glyph for a tab. See the definition for
    // why the style's own button is not used on the modern chrome.
    void applyCloseGlyph(int index);
    void applyTabCloseGlyphs();
    QStringList openUrls() const;
    void restoreTabs(const QStringList &urls, int currentIndex);
    void setHorizontalTabBarVisible(bool visible);

    // If a tab's URL already starts with `urlPrefix` (case-insensitive),
    // switches to it and returns true. Otherwise opens a new tab at `url`
    // and returns false. Used for "singleton" internal pages like
    // cloudar://settings/, so repeatedly opening Settings from the menu or
    // the New Tab tile doesn't pile up duplicate tabs.
    bool focusOrOpenTab(const QString &urlPrefix, const QUrl &url);

    // Optional. When set, every non-cloudar:// page gets the shared extensions
    // QWebChannel attached (isolated world) instead of no channel at all, so
    // enabled real (manifest.json) extensions' content scripts can reach
    // chrome.*. Must be set before any tab is opened to cover the first tab
    // too — MainWindow does this right after constructing both objects.
    void setWebExtensionManager(WebExtensionManager *manager) { m_webExtensions = manager; }

    // Optional. Every favicon Chromium decodes for a real page is handed to this
    // store, which is how the New Tab speed dial can show actual site icons.
    void setFaviconStore(FaviconStore *store) { m_favicons = store; }

    // Optional. Installs a right-click menu on every page (see PageContextMenu,
    // which explains why the platform default was not good enough). Named tabs
    // and split panes are wired the same way.
    void setContextMenuFactory(std::function<PageContextMenu *(QWebEngineView *)> factory)
    {
        m_contextMenuFactory = std::move(factory);
    }

    // --- hooks for chrome.tabs.* / chrome.scripting.executeScript, wired
    // into WebExtensionManager by MainWindow. All of these use the tab's
    // current index as its "id", the same convention MainWindow's
    // chrome.tabs.query snapshot already uses — not a stable handle across
    // tabs closing/reordering, just consistent with what's there. ---
    int openTabForExtension(const QUrl &url, bool active);
    bool updateTabForExtension(int index, const QUrl &url);
    bool closeTabForExtension(int index);
    QString urlForTabIndex(int index) const;
    void runJavaScriptInTab(int index, const QString &code,
                             std::function<void(const QString &resultJson)> callback);

    // --- Memory Saver support (see MemorySaver.h) -------------------------
    // Replaces the tab's live QWebEngineView with a static placeholder that
    // can be rebuilt cheaply, returning `true` if the tab was slept. The
    // caller (MemorySaver) owns the policy; this only performs the swap.
    bool sleepTab(int index);
    bool isTabSlept(int index) const;
    QIcon tabIcon(int index) const;
    QPixmap tabThumbnail(int index) const;

    // The hover card is a top-level window, so it survives its parent being
    // hidden or moved into a split view. Any caller that changes what is on
    // screen (the split-view toggle, window close) has to dismiss it or it
    // stays floating over unrelated content.
    void hidePreview() { dismissPreview(); }
    static QIcon defaultFavicon(const QUrl &url);

    // Re-applies the translated placeholder texts ("New Tab", sleeping hint)
    // after a language change. Titles of *real* pages belong to the site and are
    // left alone.
    void retranslate();

    // --- Tearing a tab off into another window ------------------------------
    // Everything a new window needs to reopen this tab elsewhere: the URL it is
    // showing and the title/icon the strip was showing. Returns false for a tab
    // with nothing to reopen.
    //
    // Only *state* crosses the window boundary, never the live QWebEngineView:
    // moving a WebEngine view into another top-level window re-parents its
    // rendering delegate, and the first frame after that move is a well-known
    // source of blank views. The new window loads the URL instead, which costs
    // the tab's in-page state (form input, scroll offset, media position) and
    // its per-tab back/forward list. docs/KNOWN_ISSUES.md §11 says so.
    struct TabSnapshot {
        QUrl url;
        QString title;
        QIcon icon;
    };
    bool snapshotTab(int index, TabSnapshot *snapshot) const;

    void showTabContextMenu(int index, const QPoint &globalPos);
    void reopenClosedTab();
    bool hasClosedTabs() const { return !m_closedTabUrls.isEmpty(); }
    bool reopenTabAt(int index);

signals:
    void urlChangedForCurrentTab(const QUrl &url);
    void titleChangedForCurrentTab(const QString &title);
    void tabIconChanged(int index, const QIcon &icon);
    void loadProgressForCurrentTab(int percent);
    void loadStartedForCurrentTab();
    void loadFinishedForCurrentTab(bool ok);
    void currentTabChanged(int index);
    void tabsChanged();
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    void permissionRequested(const QWebEnginePermission &permission);
#else
    void featurePermissionRequested(const QUrl &origin, QWebEnginePage::Feature feature, QWebEnginePage *page);
#endif

public slots:
    void repositionNewTabButton();
    void closeTabAt(int index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void tabInserted(int index) override;
    void tabRemoved(int index) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QWebEngineProfile *m_profile;
    QWebChannel *m_channel;
    WebExtensionManager *m_webExtensions = nullptr;
    FaviconStore *m_favicons = nullptr;
    std::function<PageContextMenu *(QWebEngineView *)> m_contextMenuFactory;
    void wireView(QWebEngineView *view);
    void animateIn(QWidget *view);

    // --- Hover thumbnail preview (Chrome-style) ---
    // Snapshots are taken opportunistically while a tab is the active one
    // (on a timer + on loadFinished) and cached here, since QtWebEngine
    // doesn't reliably render offscreen/hidden tabs — capturing "the last
    // time this tab was visible" is the same approach real browsers use
    // for background-tab thumbnails.
    QHash<QWebEngineView *, QPixmap> m_thumbnails;
    QWebEngineView *m_lastCurrentView = nullptr;
    QTimer *m_thumbnailTimer;
    TabPreviewPopup *m_previewPopup;
    QTimer *m_previewTimer;
    int m_hoveredTabIndex = -1;
    // URL of every tab that Memory Saver has slept, keyed by tab index and
    // dropped again when the tab is woken or closed.
    QHash<int, QString> m_sleptUrls;

    void captureThumbnail(QWebEngineView *view);
    void showPreviewForTab(int index);
    void dismissPreview();

    // --- "+" new-tab button, drawn right after the last tab in the tab
    // strip (Chrome/Firefox-style) instead of floating in the toolbar. ---
    QToolButton *m_newTabButton;
    QStringList m_closedTabUrls;
};
