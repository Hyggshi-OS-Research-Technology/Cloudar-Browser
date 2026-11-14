#pragma once

#include <QObject>

// Exposed to internal `cloudar://` pages only (never to public websites — see
// BrowserTabWidget, which attaches the channel exclusively when the current
// page's scheme is "cloudar"). Lets the New Tab page's tiles trigger native
// dialogs instead of navigating.
class NativeBridge : public QObject
{
    Q_OBJECT
public:
    explicit NativeBridge(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void openHistory() { emit historyRequested(); }
    void openDownloads() { emit downloadsRequested(); }
    void openSettings() { emit settingsRequested(); }
    void openExtensions() { emit extensionsRequested(); }

signals:
    void historyRequested();
    void downloadsRequested();
    void settingsRequested();
    void extensionsRequested();
};
