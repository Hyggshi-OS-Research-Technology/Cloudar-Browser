#include "MemorySaver.h"
#include "BrowserTabWidget.h"

#include <QDateTime>
#include <QSettings>
#include <QWebEngineView>

namespace {

// Level -> idle threshold. These are the numbers the settings page has been
// advertising all along ("2 hours / 1 hour / 15 minutes / 2 minutes"), so the
// implementation uses them verbatim rather than inventing a second scale.
int thresholdForLevel(const QString &level)
{
    const QString l = level.trimmed().toLower();
    if (l == "light")
        return 2 * 60 * 60 * 1000;
    if (l == "high")
        return 15 * 60 * 1000;
    if (l == "ultra")
        return 2 * 60 * 1000;
    return 60 * 60 * 1000; // medium
}

// Rough per-tab renderer footprint used only when we cannot measure one. A
// modern Chromium renderer with a couple of frames and a small heap sits in
// this range; it is an estimate, and the settings page says so.
constexpr qint64 kEstimatedTabBytes = 90ll * 1024 * 1024;

// How often the sweeper runs. One second is cheap (it only reads a timestamp
// and iterates the tab list) and makes the "Ultra" 2-minute level feel prompt.
constexpr int kTickIntervalMs = 1000;

} // namespace

MemorySaver::MemorySaver(BrowserTabWidget *tabs, QObject *parent)
    : QObject(parent), m_tabs(tabs)
{
    m_timer.setInterval(kTickIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &MemorySaver::tick);
    m_timer.start();

    m_lastActivationMs = QDateTime::currentMSecsSinceEpoch();
    reloadSettings();
}

void MemorySaver::reloadSettings()
{
    QSettings settings;
    m_enabled = settings.value("performance/memorySaverEnabled", true).toBool();
    m_level = settings.value("performance/memorySaverLevel", "medium").toString().toLower();

    // Keep the timer running even when disabled: the user can flip the switch
    // in the settings page at any time and this object has no signal from
    // SettingsBridge to wake it up again.
    if (m_enabled && !m_timer.isActive())
        m_timer.start();
}

int MemorySaver::sleptTabCount() const
{
    if (!m_tabs)
        return 0;
    int asleep = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->isTabSlept(i))
            ++asleep;
    }
    return asleep;
}

int MemorySaver::thresholdMs() const
{
    return thresholdForLevel(m_level);
}

void MemorySaver::noteTabActivated(int index)
{
    Q_UNUSED(index);
    m_lastActivationMs = QDateTime::currentMSecsSinceEpoch();

    // Waking happens in BrowserTabWidget (it owns the placeholder), so there
    // is nothing to do here beyond resetting the idle clock for the tab the
    // user just came back to.
}

void MemorySaver::tick()
{
    if (!m_enabled || !m_tabs)
        return;

    // Cheap re-read: the settings page writes QSettings directly and has no
    // back-channel into this object.
    QSettings settings;
    const bool nowEnabled = settings.value("performance/memorySaverEnabled", true).toBool();
    const QString nowLevel = settings.value("performance/memorySaverLevel", "medium").toString().toLower();
    if (nowEnabled != m_enabled || nowLevel != m_level) {
        m_enabled = nowEnabled;
        m_level = nowLevel;
        if (!m_enabled)
            return;
    }

    if (!m_enabled)
        return;
    if (m_tabs->count() <= 1)
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 idleMs = now - m_lastActivationMs;
    if (idleMs < thresholdMs())
        return;

    // The user has been sitting on one tab for longer than the threshold:
    // sleep everything else that still has a renderer.
    int slept = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (i == m_tabs->currentIndex())
            continue;
        if (m_tabs->isTabSlept(i))
            continue;
        const QString before = m_tabs->urlForTabIndex(i);
        if (m_tabs->sleepTab(i)) {
            ++slept;
            emit tabSlept(i, before);
        }
    }

    if (slept > 0) {
        // Restart the idle clock so a batch discard is not immediately
        // followed by another sweep on the next tick.
        m_lastActivationMs = now;
        reportStatistics();
    }

}

int MemorySaver::discardAllBackgroundTabs()
{
    if (!m_tabs)
        return 0;

    int slept = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (i == m_tabs->currentIndex() || m_tabs->isTabSlept(i))
            continue;
        if (m_tabs->sleepTab(i))
            ++slept;
    }

    m_lastActivationMs = QDateTime::currentMSecsSinceEpoch();
    reportStatistics();

    // Returned in MB, matching what the settings page displays.
    return static_cast<int>((static_cast<qint64>(slept) * kEstimatedTabBytes) / (1024 * 1024));
}

qint64 MemorySaver::approximatedFreedBytes() const
{
    return static_cast<qint64>(sleptTabCount()) * kEstimatedTabBytes;
}

void MemorySaver::reportStatistics()
{
    emit statisticsChanged(sleptTabCount(), approximatedFreedBytes());
}
