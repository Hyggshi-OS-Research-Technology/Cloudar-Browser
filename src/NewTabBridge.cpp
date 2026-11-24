#include "NewTabBridge.h"

#include "BookmarkManager.h"
#include "ClassicTheme.h"
#include "FaviconStore.h"
#include "HistoryManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace {

const char *kSearchKey = "browser/searchEngine";
const char *kDefaultSearch = "https://duckduckgo.com/?q=%s";

// The page shows the engine's name above the search box; deriving it from the
// template beats a second setting that can disagree with the first one.
QString engineName(const QString &searchTemplate)
{
    const QString host = QUrl(searchTemplate).host().toLower();
    if (host.contains(QLatin1String("duckduckgo")))
        return QStringLiteral("DuckDuckGo");
    if (host.contains(QLatin1String("google")))
        return QStringLiteral("Google");
    if (host.contains(QLatin1String("bing")))
        return QStringLiteral("Bing");
    if (host.contains(QLatin1String("brave")))
        return QStringLiteral("Brave Search");
    if (host.contains(QLatin1String("startpage")))
        return QStringLiteral("Startpage");
    if (host.contains(QLatin1String("ecosia")))
        return QStringLiteral("Ecosia");
    if (host.contains(QLatin1String("qwant")))
        return QStringLiteral("Qwant");
    if (host.contains(QLatin1String("mojeek")))
        return QStringLiteral("Mojeek");
    if (host.contains(QLatin1String("wikipedia")))
        return QStringLiteral("Wikipedia");
    const QString bare = host.startsWith(QLatin1String("www.")) ? host.mid(4) : host;
    // Never invent a name: an unknown engine is reported as its hostname, which
    // is a fact, or as "the configured engine" when the template has no host.
    return bare.isEmpty() ? QStringLiteral("the configured engine") : bare;
}

} // namespace

NewTabBridge::NewTabBridge(BookmarkManager *bookmarks, HistoryManager *history,
                           FaviconStore *favicons, QObject *parent)
    : QObject(parent), m_bookmarks(bookmarks), m_history(history), m_favicons(favicons)
{
    if (m_favicons) {
        connect(m_favicons, &FaviconStore::faviconReady,
                this, &NewTabBridge::faviconReady, Qt::UniqueConnection);
    }
}

QVariantMap NewTabBridge::initialState()
{
    QVariantMap out;

    // The style in force for *this run*, not the saved preference: --classic
    // must repaint the New Tab page too, and Theme::current() is the only
    // source that knows about the command-line override.
    out["chromeStyle"] = Theme::keyFor(Theme::current());

    const QString searchTemplate =
            QSettings().value(QLatin1String(kSearchKey), QLatin1String(kDefaultSearch)).toString();
    out["searchTemplate"] = searchTemplate;
    out["searchName"] = engineName(searchTemplate);
    out["version"] = QCoreApplication::applicationVersion().isEmpty()
            ? QStringLiteral(CLOUDAR_VERSION)
            : QCoreApplication::applicationVersion();

    QVariantList bookmarks;
    if (m_bookmarks) {
        for (const BookmarkEntry &entry : m_bookmarks->entries()) {
            QVariantMap obj;
            obj["title"] = entry.title;
            obj["url"] = entry.url;
            bookmarks.append(obj);
        }
    }
    out["bookmarks"] = bookmarks;

    QVariantList recent;
    if (m_history) {
        // HistoryManager keeps entries newest-first (addVisit prepends), so this
        // is a straight walk — no reversal, which would surface the oldest pages.
        const QVector<HistoryEntry> entries = m_history->entries();
        const int cap = qMin(6, entries.size());
        for (int i = 0; i < cap; ++i) {
            const HistoryEntry &entry = entries[i];
            QVariantMap obj;
            obj["title"] = entry.title;
            obj["url"] = entry.url;
            obj["visited"] = entry.visitedAt.isValid() ? entry.visitedAt.toString(Qt::ISODate)
                                                       : QString();
            recent.append(obj);
        }
    }
    out["recent"] = recent;

    return out;
}

QString NewTabBridge::faviconUrl(const QString &pageUrl)
{
    if (!m_favicons)
        return QString();
    const QUrl url = QUrl::fromUserInput(pageUrl);
    if (!url.isValid())
        return QString();
    return m_favicons->faviconFor(url);
}

void NewTabBridge::openUrl(const QString &url)
{
    const QUrl target = QUrl::fromUserInput(url.trimmed());
    if (!target.isValid() || target.isEmpty())
        return;
    emit navigationRequested(target);
}
