#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class BrowserTabWidget;

// ---------------------------------------------------------------------------
// Memory Saver.
//
// The settings page has offered "Light / Medium / High / Ultra" memory saver
// levels for a while, but nothing behind it ever discarded a tab — the hover
// cards even printed a hash-derived "Memory usage: 183 MB" that was pure
// decoration. This class is the real thing:
//
//   * It watches which tab was last active and for how long.
//   * After the level's idle threshold, a background tab is "slept": its URL
//     and title are remembered, the QWebEngineView is torn down (releasing
//     the renderer process Chromium created for it) and replaced by a
//     lightweight placeholder widget that costs a few kilobytes.
//   * Activating the tab rebuilds the real page from the remembered URL.
//
// Tearing down a QWebEnginePage is the only supported way to actually return
// a background tab's memory to the OS in QtWebEngine. The trade-off is that a
// slept tab loses in-page JS state and form input, which is the same bargain
// Chrome's own Memory Saver makes — hence the level is configurable and the
// unrecoverable state (unpublished form drafts) is called out in the settings
// page rather than left as a surprise.
// ---------------------------------------------------------------------------

class MemorySaver : public QObject
{
    Q_OBJECT
public:
    explicit MemorySaver(BrowserTabWidget *tabs, QObject *parent = nullptr);

    // Re-reads performance/memorySaverEnabled + performance/memorySaverLevel.
    void reloadSettings();

    bool enabled() const { return m_enabled; }
    QString level() const { return m_level; }

    // Idle threshold in milliseconds for the current level.
    int thresholdMs() const;

    // Number of tabs currently slept, and the estimated memory they hold right
    // now. The estimate is deliberately conservative and labelled as such in
    // the UI: QtWebEngine exposes no per-tab RSS without platform-specific
    // process inspection.
    //
    // approximatedFreedBytes() is *derived* from the live sleeping count rather
    // than accumulated at each discard, because a woken tab gets its renderer
    // back. A running total would keep claiming memory that is in use again --
    // after a few cycles the settings page would report hundreds of megabytes
    // "released" while nothing was actually retired.
    int sleptTabCount() const;
    qint64 approximatedFreedBytes() const;

    // Manual "Clean Memory Now" from the settings page: discards every
    // background tab regardless of idle time. Returns the estimated megabytes.
    int discardAllBackgroundTabs();

    // Called by the tab host whenever the user switches tabs, so the idle
    // clock restarts on the tab they actually came back to.
    void noteTabActivated(int index);

public slots:
    void tick();

signals:
    void tabSlept(int index, const QString &url);
    void tabWoken(int index, const QString &url);
    void statisticsChanged(int sleptTabs, qint64 freedBytes);

private:
    BrowserTabWidget *m_tabs;
    QTimer m_timer;
    bool m_enabled = true;
    QString m_level = QStringLiteral("medium");
    qint64 m_lastActivationMs = 0;

    void reportStatistics();
};
