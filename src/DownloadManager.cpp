#include "DownloadManager.h"

#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>

namespace {
const char *kDownloadDirKey = "browser/downloadDir";
const char *kAskLocationKey = "browser/askDownloadLocation";

QString defaultDownloadDir()
{
    // Falls back to the platform "Downloads" folder (~/Downloads on
    // Linux) the first time the app runs, before any setting is saved.
    const QString standard = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return standard.isEmpty() ? (QDir::homePath() + "/Downloads") : standard;
}

QString describe(const DownloadRecord &record)
{
    if (record.cancelled)
        return QStringLiteral("Cancelled: %1").arg(record.filename);
    if (record.completed)
        return QStringLiteral("Done: %1").arg(record.filename);
    if (record.paused)
        return QStringLiteral("Paused: %1 (%2%)").arg(record.filename).arg(record.progress);
    if (record.progress > 0)
        return QStringLiteral("Downloading: %1 (%2%)").arg(record.filename).arg(record.progress);
    return QStringLiteral("Downloading: %1").arg(record.filename);
}
}

DownloadManager::DownloadManager(QWidget *parentWidget, QObject *parent)
    : QObject(parent), m_parentWidget(parentWidget)
{
}

void DownloadManager::ensureDialog()
{
    if (m_dialog)
        return;

    m_dialog = new QDialog(m_parentWidget);
    m_dialog->setWindowTitle("Downloads");
    m_dialog->resize(420, 320);

    auto *layout = new QVBoxLayout(m_dialog);
    m_list = new QListWidget(m_dialog);
    layout->addWidget(m_list);

    auto *hint = new QLabel("Double-click a finished download to open its folder.", m_dialog);
    hint->setStyleSheet("color: #8b93a7; font-size: 11px;");
    layout->addWidget(hint);

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });

    // The dialog can be closed without destroying it, so it has to be brought
    // up to date whenever it is shown again.
    syncList();
}

void DownloadManager::showDialog()
{
    ensureDialog();
    syncList();
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
}

int DownloadManager::indexOfRequest(const QWebEngineDownloadRequest *request) const
{
    for (int i = 0; i < m_records.size(); ++i) {
        if (m_records[i].request == request)
            return i;
    }
    return -1;
}

int DownloadManager::indexOfFile(const QString &filename) const
{
    for (int i = 0; i < m_records.size(); ++i) {
        if (m_records[i].filename == filename)
            return i;
    }
    return -1;
}

DownloadRecord *DownloadManager::recordForFile(const QString &filename)
{
    const int index = indexOfFile(filename);
    return index < 0 ? nullptr : &m_records[index];
}

void DownloadManager::syncList()
{
    // The dialog is created lazily, but the change notification is not optional:
    // cloudar://downloads is a real consumer and it has no dialog to wait for.
    // Emitting only after the native list existed meant a download started
    // before anyone opened the dialog never reached the page.
    if (m_list) {
        m_list->clear();
        for (const DownloadRecord &record : m_records) {
            auto *item = new QListWidgetItem(describe(record));
            item->setData(Qt::UserRole, record.path);
            m_list->addItem(item);
        }
    }
    emit changed();
}

void DownloadManager::handleDownloadRequested(QWebEngineDownloadRequest *download)
{
    QSettings settings;
    const QString downloadsDir = settings.value(kDownloadDirKey, defaultDownloadDir()).toString();
    QDir().mkpath(downloadsDir);

    QString fileName = download->downloadFileName();
    if (fileName.isEmpty())
        fileName = "download";

    QString finalPath = downloadsDir + "/" + fileName;

    const bool askLocation = settings.value(kAskLocationKey, true).toBool();
    if (askLocation) {
        const QString chosen = QFileDialog::getSaveFileName(
            m_parentWidget, "Save File As", finalPath);
        if (chosen.isEmpty()) {
            // User cancelled the save dialog — cancel the download too
            // rather than silently saving it somewhere they didn't pick.
            download->cancel();
            return;
        }
        finalPath = chosen;
    }

    const QFileInfo info(finalPath);
    QDir().mkpath(info.absolutePath());
    download->setDownloadDirectory(info.absolutePath());
    download->setDownloadFileName(info.fileName());

    DownloadRecord record;
    record.filename = info.fileName();
    record.path = info.absoluteFilePath();
    record.request = download;
    m_records.append(record);

    // `download` outlives this handler but not necessarily the window, so the
    // pointer in the record is a QPointer and every use re-checks it.
    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, this,
            [this, download]() {
                if (!download)
                    return;
                // Matched by pointer, not by name: setDownloadFileName() renames
                // the request, so downloadFileName() and the record's filename
                // can disagree.
                const int row = indexOfRequest(download);
                DownloadRecord *rec = row < 0 ? nullptr : &m_records[row];
                if (!rec)
                    return;
                const qint64 total = download->totalBytes();
                rec->progress = total > 0
                    ? int((download->receivedBytes() * 100) / total)
                    : 0;
                if (m_list)
                    syncList();
            });

    connect(download, &QWebEngineDownloadRequest::stateChanged, this,
            [this, download](QWebEngineDownloadRequest::DownloadState state) {
                // Matched by pointer, not by name: setDownloadFileName() renames
                // the request, so downloadFileName() and the record's filename
                // can disagree.
                const int row = indexOfRequest(download);
                DownloadRecord *rec = row < 0 ? nullptr : &m_records[row];
                if (!rec)
                    return;
                switch (state) {
                case QWebEngineDownloadRequest::DownloadCompleted:
                    rec->completed = true;
                    rec->progress = 100;
                    rec->paused = false;
                    rec->request = nullptr;
                    // Show the user what just landed instead of leaving
                    // them to go find it themselves.
                    QDesktopServices::openUrl(
                        QUrl::fromLocalFile(QFileInfo(rec->path).absolutePath()));
                    showDialog();
                    break;
                case QWebEngineDownloadRequest::DownloadCancelled:
                case QWebEngineDownloadRequest::DownloadInterrupted:
                    // Both end the transfer; they differ only in
                    // interruptReason(), which the page does not display.
                    rec->cancelled = true;
                    rec->paused = false;
                    rec->request = nullptr;
                    break;
                default:
                    // DownloadState has five values and none of them is
                    // "paused": pausing leaves the state at DownloadInProgress
                    // and flips isPaused() instead.
                    rec->paused = download->isPaused();
                    break;
                }
                if (m_list)
                    syncList();
            });

    // pause()/resume() flip isPaused() without changing the state, so without
    // this a paused download would keep rendering as "Downloading 42%".
    connect(download, &QWebEngineDownloadRequest::isPausedChanged, this,
            [this, download]() {
                const int row = indexOfRequest(download);
                if (row < 0)
                    return;
                m_records[row].paused = download->isPaused();
                if (m_list)
                    syncList();
            });

    download->accept();
    syncList();
    showDialog();
}

void DownloadManager::openDownload(const QString &filename)
{
    const int index = indexOfFile(filename);
    if (index < 0)
        return;
    const DownloadRecord &record = m_records[index];
    if (!record.completed)
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(record.path));
}

void DownloadManager::showInFolder(const QString &filename)
{
    const int index = indexOfFile(filename);
    if (index < 0)
        return;
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(m_records[index].path).absolutePath()));
}

void DownloadManager::pauseDownload(const QString &filename)
{
    DownloadRecord *record = recordForFile(filename);
    if (!record || !record->request || record->completed || record->cancelled)
        return;
    // isPausedChanged() updates the record; setting the flag here too would
    // claim success even if the engine refused to pause.
    record->request->pause();
}

void DownloadManager::resumeDownload(const QString &filename)
{
    DownloadRecord *record = recordForFile(filename);
    if (!record || !record->request || record->completed || record->cancelled)
        return;
    record->request->resume();
}

void DownloadManager::cancelDownload(const QString &filename)
{
    DownloadRecord *record = recordForFile(filename);
    if (!record || !record->request)
        return;
    record->request->cancel();
    record->cancelled = true;
    record->paused = false;
    if (m_list)
        syncList();
}
