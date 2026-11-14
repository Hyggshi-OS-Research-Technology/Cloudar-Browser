#include "SessionManager.h"

#include <QSettings>

namespace {
constexpr auto kUrlsKey = "session/openUrls";
constexpr auto kCurrentTabKey = "session/currentTab";
constexpr auto kRestoreKey = "browser/restoreSession";
}

SessionManager::SessionManager(QObject *parent) : QObject(parent) {}

QStringList SessionManager::urls() const
{
    return QSettings().value(kUrlsKey).toStringList();
}

int SessionManager::currentIndex() const
{
    return QSettings().value(kCurrentTabKey, 0).toInt();
}

bool SessionManager::restoreEnabled() const
{
    return QSettings().value(kRestoreKey, true).toBool();
}

void SessionManager::save(const QStringList &urls, int currentIndex)
{
    QSettings settings;
    settings.setValue(kUrlsKey, urls);
    settings.setValue(kCurrentTabKey, currentIndex);
}

void SessionManager::clear()
{
    QSettings settings;
    settings.remove(kUrlsKey);
    settings.remove(kCurrentTabKey);
}
