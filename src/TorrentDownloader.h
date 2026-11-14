#ifndef TORRENT_DOWNLOADER_H
#define TORRENT_DOWNLOADER_H

#include <QObject>
#include <QString>

class QMainWindow;

class TorrentDownloaderBridge : public QObject {
    Q_OBJECT

public:
    explicit TorrentDownloaderBridge(QMainWindow *window = nullptr, QObject *parent = nullptr);

    Q_INVOKABLE bool isAvailable();
    Q_INVOKABLE void downloadMagnet(const QString &magnetUri, const QString &suggestedName = "");

signals:
    void downloadStarted(const QString &url);
    void downloadProgress(const QString &url, double percent, const QString &speedStr);
    void downloadFinished(const QString &url, bool success, const QString &pathOrError);

private:
    QMainWindow *m_window = nullptr;
};

#endif // TORRENT_DOWNLOADER_H
