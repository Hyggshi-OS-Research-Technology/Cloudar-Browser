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

    QIcon tabIcon(int index) const;
    QPixmap tabThumbnail(int index) const;
    static QIcon defaultFavicon(const QUrl &url);

    void showTabContextMenu(int index, const QPoint &globalPos);
    void reopenClosedTab();
    bool hasClosedTabs() const { return !m_closedTabUrls.isEmpty(); }

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

    void captureThumbnail(QWebEngineView *view);
    void showPreviewForTab(int index);
    void hidePreview();

    // --- "+" new-tab button, drawn right after the last tab in the tab
    // strip (Chrome/Firefox-style) instead of floating in the toolbar. ---
    QToolButton *m_newTabButton;
    QStringList m_closedTabUrls;
};
