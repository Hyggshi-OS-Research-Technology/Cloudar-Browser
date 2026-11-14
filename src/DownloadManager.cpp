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

    connect(m_list, &QListWidget::itemDoubleClicked, this, [](QListWidgetItem *item) {
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });
}

void DownloadManager::showDialog()
{
    ensureDialog();
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
}

void DownloadManager::handleDownloadRequested(QWebEngineDownloadRequest *download)
{
    ensureDialog();

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

    auto *item = new QListWidgetItem(QString("Downloading: %1").arg(info.fileName()));
    item->setData(Qt::UserRole, finalPath);
    m_list->addItem(item);

    connect(download, &QWebEngineDownloadRequest::stateChanged, this,
            [this, item, finalPath, download](QWebEngineDownloadRequest::DownloadState state) {
                const QFileInfo info(finalPath);
                switch (state) {
                case QWebEngineDownloadRequest::DownloadCompleted:
                    item->setText(QString("Done: %1").arg(info.fileName()));
                    // Show the user what just landed instead of leaving
                    // them to go find it themselves.
                    QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
                    showDialog();
                    break;
                case QWebEngineDownloadRequest::DownloadCancelled:
                    item->setText(QString("Cancelled: %1").arg(info.fileName()));
                    break;
                case QWebEngineDownloadRequest::DownloadInterrupted:
                    item->setText(QString("Failed: %1 (%2)")
                                      .arg(info.fileName(), download->interruptReasonString()));
                    break;
                default:
                    break;
                }
            });

    download->accept();
    showDialog();
}
