#pragma once

#include <QObject>
#include <QDialog>
#include <QListWidget>
#include <QVector>
#include <QPointer>
#include <QWebEngineDownloadRequest>

// One row of the downloads list, kept separately from the QListWidget so the
// cloudar://downloads page can read it: the page is the primary UI for
// downloads now, and a widget-only model would have meant the page could show
// nothing but an empty state.
struct DownloadRecord {
    QString filename;
    QString path;
    int progress = 0;
    bool completed = false;
    bool cancelled = false;
    bool paused = false;
    QPointer<QWebEngineDownloadRequest> request; // null once finished/removed
};

// Tracks active/completed downloads and shows a minimal list dialog.
// Kept intentionally simple: no thumbnails, no per-chunk UI.
//
// Behavior (configurable in Settings, both on by default):
//  - Files land in QSettings("browser/downloadDir"), default ~/Downloads.
//  - Before each download starts, a native "Save As" dialog lets the user
//    pick/rename the destination (QSettings "browser/askDownloadLocation").
//  - When a download finishes, its folder is opened so the user sees the
//    file land instead of having to go hunting for it.
//
// Pause/resume are real: QWebEngineDownloadRequest::pause()/resume() are the
// engine's own calls, and an entry whose request has gone away is reported as
// not pausable rather than pretending.
class DownloadManager : public QObject
{
    Q_OBJECT
public:
    // `parentWidget` parents the download list dialog and the native
    // save-location file dialog.
    explicit DownloadManager(QWidget *parentWidget, QObject *parent = nullptr);

    void handleDownloadRequested(QWebEngineDownloadRequest *download);
    void showDialog();

    QVector<DownloadRecord> records() const { return m_records; }

    // Take a filename rather than an index: the cloudar://downloads page
    // addresses rows the same way the native list does, by name, and an index
    // would be invalidated by any concurrent download starting.
    void openDownload(const QString &filename);
    void showInFolder(const QString &filename);
    void pauseDownload(const QString &filename);
    void resumeDownload(const QString &filename);
    void cancelDownload(const QString &filename);

signals:
    void changed();

private:
    QWidget *m_parentWidget;
    QDialog *m_dialog = nullptr;
    QListWidget *m_list = nullptr;
    QVector<DownloadRecord> m_records;

    void ensureDialog();
    void syncList();
    int indexOfRequest(const QWebEngineDownloadRequest *request) const;
    int indexOfFile(const QString &filename) const;
    DownloadRecord *recordForFile(const QString &filename);
};
