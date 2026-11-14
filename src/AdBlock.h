#ifndef AD_BLOCK_H
#define AD_BLOCK_H

#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include <QSet>
#include <QMutex>

class AdBlockInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT

public:
    explicit AdBlockInterceptor(QObject *parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    int getBlockedCount() const { return m_blockedCount; }
    void resetCount();

signals:
    void blockedCountChanged(int count);

private:
    bool shouldBlock(const QUrl &url) const;

    bool m_enabled = true;
    int m_blockedCount = 0;
    mutable QMutex m_mutex;
    QSet<QString> m_adDomains;
    QSet<QString> m_adPathPatterns;
};

#endif // AD_BLOCK_H
