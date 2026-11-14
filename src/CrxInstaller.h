#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>

class QNetworkReply;

// Downloads a CRX/ZIP package and installs its ZIP payload as an unpacked
// extension. CRX3's protobuf header is deliberately treated as opaque: this
// runtime has no Chrome Web Store trust root, so it validates the container
// structure but cannot make Chrome's publisher-signature guarantee.
class CrxInstaller : public QObject
{
    Q_OBJECT
public:
    static constexpr qint64 MaxPackageBytes = 100LL * 1024 * 1024;
    static constexpr qint64 MaxUnpackedBytes = 200LL * 1024 * 1024;

    explicit CrxInstaller(const QString &extensionsRoot, QObject *parent = nullptr);

    void downloadAndInstall(const QUrl &url);
    void installFile(const QString &filePath);

signals:
    void progress(qint64 received, qint64 total);
    void statusChanged(const QString &message);
    void installed(const QString &extensionDir);
    void failed(const QString &message);

private:
    QString m_extensionsRoot;
    QNetworkAccessManager m_network;
    QNetworkReply *m_reply = nullptr;

    void installPackage(const QByteArray &package, const QString &sourceLabel);
    bool extractZip(const QByteArray &zip, const QString &destination, QString *error) const;
    static bool crx3Payload(const QByteArray &package, QByteArray *zip, QString *error);
    static QString extensionDirectoryName(const QString &name);
};
