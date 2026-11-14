#pragma once

#include <QObject>
#include <QStringList>

// Saves only restorable browser state (open page URLs and selected tab).
// WebEngine remains responsible for per-page navigation history and storage.
class SessionManager : public QObject
{
    Q_OBJECT
public:
    explicit SessionManager(QObject *parent = nullptr);

    QStringList urls() const;
    int currentIndex() const;
    bool restoreEnabled() const;
    void save(const QStringList &urls, int currentIndex);
    void clear();
};
