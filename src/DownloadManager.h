#pragma once

#include <QObject>
#include <QDialog>
#include <QListWidget>
#include <QWebEngineDownloadRequest>

// Tracks active/completed downloads and shows a minimal list dialog.
// Kept intentionally simple: no thumbnails, no pause/resume UI — just
// enough to know what downloaded and where it went.
//
// Behavior (configurable in Settings, both on by default):
//  - Files land in QSettings("browser/downloadDir"), default ~/Downloads.
//  - Before each download starts, a native "Save As" dialog lets the user
//    pick/rename the destination (QSettings "browser/askDownloadLocation").
//  - When a download finishes, its folder is opened so the user sees the
//    file land instead of having to go hunting for it.
class DownloadManager : public QObject
{
    Q_OBJECT
public:
    // `parentWidget` parents the download list dialog and the native
    // save-location file dialog.
    explicit DownloadManager(QWidget *parentWidget, QObject *parent = nullptr);

    void handleDownloadRequested(QWebEngineDownloadRequest *download);
    void showDialog();

private:
    QWidget *m_parentWidget;
    QDialog *m_dialog = nullptr;
    QListWidget *m_list = nullptr;

    void ensureDialog();
};
