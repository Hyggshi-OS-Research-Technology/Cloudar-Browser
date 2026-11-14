#include "YouTubeDownloader.h"
#include "BrowserResources.h"

#include <QStandardPaths>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>

YouTubeDownloaderBridge::YouTubeDownloaderBridge(QMainWindow *window, QObject *parent)
    : QObject(parent), m_window(window) {
}

bool YouTubeDownloaderBridge::isAvailable() {
    return !QStandardPaths::findExecutable("yt-dlp").isEmpty();
}

QString YouTubeDownloaderBridge::getDownloadDir() {
    QJsonObject settings = BrowserResources::loadJsonObject(BrowserResources::getSettingsFile(), BrowserResources::getDefaultSettings());
    QString dir = settings.value("download_location").toString();
    if (dir.isEmpty() || !QDir(dir).exists()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    }
    return dir;
}

void YouTubeDownloaderBridge::requestVideoInfo(const QString &url) {
    if (url.trimmed().isEmpty()) return;

    QString ytDlp = QStandardPaths::findExecutable("yt-dlp");
    if (ytDlp.isEmpty()) {
        emit videoInfoFailed(url, "yt-dlp is not installed.");
        return;
    }

    QProcess *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, proc, url](int exitCode, QProcess::ExitStatus status) {
        proc->deleteLater();
        if (exitCode == 0 && status == QProcess::NormalExit) {
            QString out = QString::fromUtf8(proc->readAllStandardOutput());
            emit videoInfoReady(url, out);
        } else {
            QString err = QString::fromUtf8(proc->readAllStandardError());
            emit videoInfoFailed(url, err.isEmpty() ? "Failed to get video info" : err);
        }
    });

    proc->start(ytDlp, QStringList() << "--dump-json" << "--no-playlist" << url);
}

void YouTubeDownloaderBridge::downloadVideo(const QString &url, const QString &suggestedTitle, const QString &quality, const QString &container) {
    Q_UNUSED(suggestedTitle);
    if (url.trimmed().isEmpty() || m_activeDownloads.contains(url)) return;

    QString ytDlp = QStandardPaths::findExecutable("yt-dlp");
    if (ytDlp.isEmpty()) {
        emit downloadFinished(url, false, "yt-dlp is not installed.");
        return;
    }

    m_activeDownloads.insert(url);
    emit downloadStarted(url);

    QString outDir = getDownloadDir();
    QString outTemplate = QDir(outDir).filePath("%(title)s.%(ext)s");

    QStringList args;
    args << "--newline" << "-o" << outTemplate;

    if (quality == "audio") {
        args << "-x" << "--audio-format" << (container.isEmpty() ? "mp3" : container);
    } else if (quality == "1080p") {
        args << "-f" << "bestvideo[height<=1080]+bestaudio/best[height<=1080]";
    } else if (quality == "720p") {
        args << "-f" << "bestvideo[height<=720]+bestaudio/best[height<=720]";
    } else if (quality == "360p") {
        args << "-f" << "bestvideo[height<=360]+bestaudio/best[height<=360]";
    }

    if (!container.isEmpty() && quality != "audio") {
        args << "--merge-output-format" << container;
    }

    args << url;

    QProcess *proc = new QProcess(this);
    connect(proc, &QProcess::readyReadStandardOutput, [this, proc, url]() {
        QString line = QString::fromUtf8(proc->readAllStandardOutput());
        // e.g. [download]  45.2% of 20.00MiB at 2.50MiB/s
        QRegularExpression re(R"(\[download\]\s+([\d\.]+)%\s+of\s+([^\s]+)\s+at\s+([^\s]+))");
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            double percent = match.captured(1).toDouble();
            QString speed = match.captured(3);
            emit downloadProgress(url, percent, speed);
        }
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, proc, url, outDir](int exitCode, QProcess::ExitStatus status) {
        proc->deleteLater();
        m_activeDownloads.remove(url);
        if (exitCode == 0 && status == QProcess::NormalExit) {
            emit downloadFinished(url, true, outDir);
        } else {
            QString err = QString::fromUtf8(proc->readAllStandardError());
            emit downloadFinished(url, false, err.isEmpty() ? "Download failed" : err);
        }
    });

    proc->start(ytDlp, args);
}
