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
class LightSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT
public:
    explicit LightSchemeHandler(QObject *parent = nullptr);

    void requestStarted(QWebEngineUrlRequestJob *job) override;

    static void registerScheme(); // call once, before QApplication is constructed

private:
    void serveResource(QWebEngineUrlRequestJob *job, const QString &resourcePath,
                        const QByteArray &mimeType);
};
