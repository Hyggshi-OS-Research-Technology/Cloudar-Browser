#ifndef YOUTUBE_DOWNLOADER_H
#define YOUTUBE_DOWNLOADER_H

#include <QObject>
#include <QString>
#include <QSet>

class QMainWindow;

class YouTubeDownloaderBridge : public QObject {
    Q_OBJECT

public:
    explicit YouTubeDownloaderBridge(QMainWindow *window = nullptr, QObject *parent = nullptr);

    Q_INVOKABLE bool isAvailable();
    Q_INVOKABLE void requestVideoInfo(const QString &url);
    Q_INVOKABLE void downloadVideo(const QString &url, const QString &suggestedTitle = "", const QString &quality = "", const QString &container = "");

signals:
    void downloadStarted(const QString &url);
    void downloadProgress(const QString &url, double percent, const QString &speedStr);
    void downloadFinished(const QString &url, bool success, const QString &pathOrError);
    void videoInfoReady(const QString &url, const QString &jsonInfo);
    void videoInfoFailed(const QString &url, const QString &error);

private:
    QString getDownloadDir();

    QMainWindow *m_window = nullptr;
    QSet<QString> m_activeDownloads;
};

#endif // YOUTUBE_DOWNLOADER_H
