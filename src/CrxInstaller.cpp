#include "CrxInstaller.h"

#include "WebExtensionManifest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>
#include <QRegularExpression>

#include <zlib.h>

namespace {
constexpr quint32 LocalHeader = 0x04034b50;
constexpr quint32 CentralHeader = 0x02014b50;
constexpr quint32 EndOfCentralDirectory = 0x06054b50;

bool readU16(const QByteArray &data, qsizetype offset, quint16 *value)
{
    if (offset < 0 || offset + 2 > data.size()) return false;
    const auto *p = reinterpret_cast<const uchar *>(data.constData() + offset);
    *value = quint16(p[0]) | (quint16(p[1]) << 8);
    return true;
}

bool readU32(const QByteArray &data, qsizetype offset, quint32 *value)
{
    if (offset < 0 || offset + 4 > data.size()) return false;
    const auto *p = reinterpret_cast<const uchar *>(data.constData() + offset);
    *value = quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
    return true;
}

bool safeZipPath(const QString &path)
{
    if (path.isEmpty() || path.startsWith('/') || path.startsWith('\\') || path.contains('\0')) return false;
    if (QDir::isAbsolutePath(path) || QRegularExpression(QStringLiteral("^[A-Za-z]:")).match(path).hasMatch()) return false;
    for (const QString &part : path.split('/', Qt::SkipEmptyParts))
        if (part == "." || part == "..") return false;
    return true;
}

bool inflateRaw(const QByteArray &input, QByteArray *output, quint32 expectedSize)
{
    if (expectedSize > CrxInstaller::MaxUnpackedBytes) return false;
    output->resize(int(expectedSize));
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
    stream.avail_in = uInt(input.size());
    stream.next_out = reinterpret_cast<Bytef *>(output->data());
    stream.avail_out = uInt(output->size());
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) return false;
    const int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    return result == Z_STREAM_END && stream.total_out == expectedSize;
}
}

CrxInstaller::CrxInstaller(const QString &extensionsRoot, QObject *parent)
    : QObject(parent), m_extensionsRoot(extensionsRoot)
{
    QDir().mkpath(m_extensionsRoot);
}

void CrxInstaller::downloadAndInstall(const QUrl &url)
{
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")) {
        emit failed("Extension URL must use HTTP or HTTPS.");
        return;
    }
    if (m_reply) {
        emit failed("An extension download is already in progress.");
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network.get(request);
    emit statusChanged("Downloading " + url.toDisplayString());
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        emit progress(received, total);
        if (m_reply && received > MaxPackageBytes)
            m_reply->abort();
    });
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        const QByteArray bytes = reply->readAll();
        const QString error = reply->errorString();
        const bool ok = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();
        if (!ok) { emit failed("Download failed: " + error); return; }
        if (bytes.size() > MaxPackageBytes) { emit failed("Package exceeds the 100 MiB limit."); return; }
        installPackage(bytes, "download");
    });
}

void CrxInstaller::installFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) { emit failed("Cannot read package: " + file.errorString()); return; }
    if (file.size() > MaxPackageBytes) { emit failed("Package exceeds the 100 MiB limit."); return; }
    installPackage(file.readAll(), QFileInfo(file).fileName());
}

bool CrxInstaller::crx3Payload(const QByteArray &package, QByteArray *zip, QString *error)
{
    if (package.size() >= 4 && package.left(4) == "PK\x03\x04") { *zip = package; return true; }
    if (package.size() < 12 || package.left(4) != "Cr24") { *error = "Not a ZIP package or CRX file."; return false; }
    quint32 version = 0, headerSize = 0;
    if (!readU32(package, 4, &version) || !readU32(package, 8, &headerSize)) { *error = "Malformed CRX header."; return false; }
    if (version != 3) { *error = QString("Unsupported CRX version %1 (only CRX3 is accepted).").arg(version); return false; }
    const qint64 offset = 12LL + headerSize;
    if (headerSize == 0 || offset > package.size() || package.mid(offset, 4) != "PK\x03\x04") {
        *error = "Malformed CRX3 header or missing ZIP payload."; return false;
    }
    *zip = package.mid(offset);
    return true;
}

QString CrxInstaller::extensionDirectoryName(const QString &name)
{
    QString slug = name.toLower().trimmed();
    slug.replace(QRegularExpression("[^a-z0-9._-]+"), "-");
    slug.remove(QRegularExpression("^-+|-+$"));
    return slug.isEmpty() ? "extension" : slug.left(64);
}

void CrxInstaller::installPackage(const QByteArray &package, const QString &sourceLabel)
{
    QByteArray zip;
    QString error;
    if (!crx3Payload(package, &zip, &error)) { emit failed(error); return; }

    const QString staging = QDir(m_extensionsRoot).filePath(".install-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QDir().mkpath(staging);
    emit statusChanged("Extracting " + sourceLabel + "…");
    if (!extractZip(zip, staging, &error)) { QDir(staging).removeRecursively(); emit failed(error); return; }

    WebExtensionManifest manifest = WebExtensionManifest::parse(staging);
    if (!manifest.valid) { QDir(staging).removeRecursively(); emit failed("Package does not contain a valid root manifest: " + manifest.error); return; }
    QString leaf = extensionDirectoryName(manifest.name);
    QString target = QDir(m_extensionsRoot).filePath(leaf);
    for (int n = 2; QFileInfo::exists(target); ++n)
        target = QDir(m_extensionsRoot).filePath(leaf + "-" + QString::number(n));
    if (!QDir().rename(staging, target)) { QDir(staging).removeRecursively(); emit failed("Could not move extracted extension into its install folder."); return; }
    emit statusChanged("Installed " + manifest.name + ".");
    emit installed(target);
}

bool CrxInstaller::extractZip(const QByteArray &zip, const QString &destination, QString *error) const
{
    // EOCD is within the final 65,557 bytes (maximum ZIP comment included).
    const qsizetype begin = qMax<qsizetype>(0, zip.size() - 65557);
    qsizetype eocd = -1;
    for (qsizetype i = zip.size() - 22; i >= begin; --i) {
        quint32 signature = 0;
        if (readU32(zip, i, &signature) && signature == EndOfCentralDirectory) { eocd = i; break; }
    }
    quint16 entries = 0; quint32 directorySize = 0, directoryOffset = 0;
    if (eocd < 0 || !readU16(zip, eocd + 10, &entries) || !readU32(zip, eocd + 12, &directorySize)
        || !readU32(zip, eocd + 16, &directoryOffset) || entries > 10000
        || qint64(directoryOffset) + directorySize > zip.size()) { *error = "Invalid or unsupported ZIP central directory."; return false; }

    qsizetype cursor = directoryOffset;
    qint64 totalUnpacked = 0;
    for (quint16 index = 0; index < entries; ++index) {
        quint32 signature = 0, compressedSize = 0, uncompressedSize = 0, localOffset = 0;
        quint16 flags = 0, method = 0, nameLength = 0, extraLength = 0, commentLength = 0;
        if (!readU32(zip, cursor, &signature) || signature != CentralHeader || !readU16(zip, cursor + 8, &flags)
            || !readU16(zip, cursor + 10, &method) || !readU32(zip, cursor + 20, &compressedSize)
            || !readU32(zip, cursor + 24, &uncompressedSize) || !readU16(zip, cursor + 28, &nameLength)
            || !readU16(zip, cursor + 30, &extraLength) || !readU16(zip, cursor + 32, &commentLength)
            || !readU32(zip, cursor + 42, &localOffset) || cursor + 46LL + nameLength + extraLength + commentLength > zip.size()) {
            *error = "Malformed ZIP entry."; return false;
        }
        const QString name = QString::fromUtf8(zip.mid(cursor + 46, nameLength));
        cursor += 46 + nameLength + extraLength + commentLength;
        if (!safeZipPath(name)) { *error = "ZIP contains an unsafe file path."; return false; }
        if (flags & 0x1 || (method != 0 && method != 8) || compressedSize > MaxPackageBytes || uncompressedSize > MaxUnpackedBytes
            || (totalUnpacked += uncompressedSize) > MaxUnpackedBytes) { *error = "ZIP uses unsupported encryption/compression or exceeds limits."; return false; }
        if (name.endsWith('/')) { QDir().mkpath(QDir(destination).filePath(name)); continue; }

        quint32 localSignature = 0; quint16 localNameLength = 0, localExtraLength = 0;
        if (!readU32(zip, localOffset, &localSignature) || localSignature != LocalHeader || !readU16(zip, localOffset + 26, &localNameLength)
            || !readU16(zip, localOffset + 28, &localExtraLength)) { *error = "ZIP local header is invalid."; return false; }
        const qint64 payloadOffset = qint64(localOffset) + 30 + localNameLength + localExtraLength;
        if (payloadOffset + compressedSize > zip.size()) { *error = "ZIP entry payload is truncated."; return false; }
        const QByteArray compressed = zip.mid(payloadOffset, compressedSize);
        QByteArray contents;
        if (method == 0) { if (compressedSize != uncompressedSize) { *error = "Invalid stored ZIP entry."; return false; } contents = compressed; }
        else if (!inflateRaw(compressed, &contents, uncompressedSize)) { *error = "Could not decompress ZIP entry."; return false; }

        const QString outputPath = QDir(destination).filePath(name);
        if (!QDir().mkpath(QFileInfo(outputPath).dir().absolutePath())) { *error = "Could not create extension directory."; return false; }
        QFile output(outputPath);
        if (!output.open(QIODevice::WriteOnly) || output.write(contents) != contents.size()) { *error = "Could not write extracted extension file."; return false; }
    }
    return true;
}
