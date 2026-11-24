#pragma once

#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineUrlRequestJob>

// Serves internal pages under the cloudar:// scheme:
//   cloudar://newtab, cloudar://new       -> the New Tab page
//   cloudar://settings, cloudar://Settings -> the Settings page (any path under
//                                            the host, e.g. cloudar://settings/privacy,
//                                            deep-links to a section — see settings.html)
//   cloudar://about, cloudar://version    -> about/version page
//   cloudar://flags, cloudar://bookmarks, cloudar://history, cloudar://downloads, cloudar://extensions, cloudar://permissions
// Content is bundled via Qt resources (resources.qrc), so it works the
// same whether the app is installed as .deb, .AppImage, or on Windows.
class FaviconStore;

class LightSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT
public:
    explicit LightSchemeHandler(QObject *parent = nullptr);

    // Optional. When set, cloudar://favicon/?u=<page url> serves the favicon that
    // FaviconStore cached for that page's host. Without it the host 404s, which
    // is the correct answer when nothing has been stored.
    void setFaviconStore(FaviconStore *store) { m_favicons = store; }

    void requestStarted(QWebEngineUrlRequestJob *job) override;

    static void registerScheme(); // call once, before QApplication is constructed

private:
    void serveResource(QWebEngineUrlRequestJob *job, const QString &resourcePath,
                        const QByteArray &mimeType);
    void serveBytes(QWebEngineUrlRequestJob *job, const QByteArray &data,
                    const QByteArray &mimeType);
    // cloudar://newtab/assets/... -> the New Tab page's stylesheet/script files.
    void serveNewTabAsset(QWebEngineUrlRequestJob *job, const QString &path);
    // cloudar://newtab/favicon?u=... and cloudar://favicon/?u=...
    void serveFavicon(QWebEngineUrlRequestJob *job);

    FaviconStore *m_favicons = nullptr;
};
