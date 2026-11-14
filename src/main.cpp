#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>
#include <QIcon>

#include "MainWindow.h"
#include "LightSchemeHandler.h"

int main(int argc, char *argv[])
{
    // Keep cache usage bounded without weakening Chromium's sandbox or
    // process isolation. Never add --no-sandbox here merely to simplify a
    // local build: WebEngine's security boundary is part of the browser.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            "--disable-gpu-shader-disk-cache --disk-cache-size=52428800 --autoplay-policy=no-user-gesture-required");

    // Must run before QApplication is constructed.
    LightSchemeHandler::registerScheme();

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icon.png"));
    QApplication::setApplicationName("Cloudar Browser");
    QApplication::setOrganizationName("Cloudar");
    QApplication::setApplicationVersion("0.1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Cloudar Browser - A modern Qt6/WebEngine browser");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("url", "Optional URL to open on startup");
    parser.process(app);

    // Ensure the profile data directory exists (history/cookies/downloads live here)
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    MainWindow window;
    window.resize(1200, 800);

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        window.openUrl(QUrl::fromUserInput(args.first()));
    }
    // Otherwise MainWindow already opened the user's configured home page
    // (cloudar://newtab by default) during construction.

    window.show();
    return app.exec();
}
