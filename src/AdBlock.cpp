#include "AdBlock.h"
#include "BrowserResources.h"

AdBlockInterceptor::AdBlockInterceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent), m_enabled(true), m_blockedCount(0) {
    m_adDomains = BrowserResources::getTrackingDomains();

    static const QSet<QString> extraAdDomains = {
        "doubleclick.net", "googlesyndication.com", "googleadservices.com",
        "adservice.google.com", "adnxs.com", "advertising.com", "adsafeprotected.com",
        "moatads.com", "scorecardresearch.com", "outbrain.com", "taboola.com",
        "criteo.com", "criteo.net", "rubiconproject.com", "pubmatic.com", "openx.net",
        "adform.net", "amazon-adsystem.com", "media.net", "adsrvr.org", "bidswitch.net",
        "casalemedia.com", "yieldmo.com", "3lift.com", "sharethrough.com", "adroll.com",
        "teads.tv", "spotxchange.com", "smartadserver.com", "gumgum.com"
    };
    m_adDomains.unite(extraAdDomains);

    m_adPathPatterns = {
        "/pagead/", "/api/stats/ads", "ptracking", "/doubleclick/", "/adservice/"
    };
}

void AdBlockInterceptor::resetCount() {
    QMutexLocker locker(&m_mutex);
    m_blockedCount = 0;
    emit blockedCountChanged(m_blockedCount);
}

bool AdBlockInterceptor::shouldBlock(const QUrl &url) const {
    QString host = url.host().toLower();
    QString path = url.path().toLower();

    // Check host suffix against domain list
    for (const QString &domain : m_adDomains) {
        if (host == domain || host.endsWith("." + domain)) {
            return true;
        }
    }

    // Check common ad paths
    for (const QString &pattern : m_adPathPatterns) {
        if (path.contains(pattern)) {
            return true;
        }
    }

    return false;
}

void AdBlockInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info) {
    if (!m_enabled) return;

    QUrl url = info.requestUrl();
    if (shouldBlock(url)) {
        info.block(true);

        int count = 0;
        {
            QMutexLocker locker(&m_mutex);
            m_blockedCount++;
            count = m_blockedCount;
        }
        emit blockedCountChanged(count);
    }
}
