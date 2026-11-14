#include "TorrentDownloader.h"
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>

TorrentDownloaderBridge::TorrentDownloaderBridge(QMainWindow *window, QObject *parent)
    : QObject(parent), m_window(window) {
}

bool TorrentDownloaderBridge::isAvailable() {
    // If a system torrent client or handler exists
    return true;
}

void TorrentDownloaderBridge::downloadMagnet(const QString &magnetUri, const QString &suggestedName) {
    Q_UNUSED(suggestedName);
    if (!magnetUri.startsWith("magnet:")) {
        emit downloadFinished(magnetUri, false, "Invalid magnet URI");
        return;
    }

    emit downloadStarted(magnetUri);
    bool ok = QDesktopServices::openUrl(QUrl(magnetUri));
    emit downloadFinished(magnetUri, ok, ok ? "Opened in external torrent client" : "Failed to launch torrent client");
}
