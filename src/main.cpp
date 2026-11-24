#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>
#include <QIcon>
#include <QSettings>
#include <cstdio>

#include "MainWindow.h"
#include "LightSchemeHandler.h"
#include "PerformanceProfile.h"
#include "ClassicTheme.h"

int main(int argc, char *argv[])
{
    // Organization/application identity first: QSettings cannot resolve a file
    // without it, and the performance tier below reads the saved preference.
    // Qt does not overwrite these from the executable name when they are
    // already set, so the explicit form further down stays authoritative.
    QApplication::setApplicationName("Cloudar Browser");
    QApplication::setOrganizationName("Cloudar");

    // CPU/GPU tuning has to happen before QApplication exists: Qt reads
    // QT_OPENGL at platform-plugin load time, and QtWebEngine reads
    // QTWEBENGINE_CHROMIUM_FLAGS the first time a profile is created. See
    // PerformanceProfile.cpp for what each tier actually sets, and
    // docs/CPU_COMPATIBILITY.md for the hardware floor.
    PerformanceProfile::applyEnvironment();

    // Qt6 in general (and QtWebEngine in particular) no longer supports the
    // XRender-only X11 servers and pre-SSE3 CPUs that Qt5-era builds ran on.
    // The tier system above is what makes the remaining supported range
    // usable; it cannot widen the engine's own floor. That distinction is
    // documented rather than silently papered over with --no-sandbox.

    // --help and --version must work without a display. They are answered
    // before QApplication exists: constructing a QApplication with no X11 /
    // Wayland session aborts the process (SIGABRT), which is exactly what
    // `cloudar-browser --version` did on a headless CI runner.
    // A hand-rolled scan rather than QCommandLineParser: the parser's help and
    // version handling routes through QCoreApplication, which does not exist
    // yet at this point.
    for (int i = 1; i < argc; ++i) {
        const QByteArray arg(argv[i]);
        if (arg == "-v" || arg == "--version") {
            fputs("Cloudar Browser " CLOUDAR_VERSION "\n", stdout);
            return 0;
        }
        if (arg == "-h" || arg == "--help" || arg == "--help-all") {
            fputs("Cloudar Browser " CLOUDAR_VERSION "\n"
                  "A Qt6/QtWebEngine browser with a Chrome 2009 classic interface.\n\n"
                  "Usage: cloudar-browser [options] [url]\n\n"
                  "Options:\n"
                  "  -h, --help              Show this help.\n"
                  "  -v, --version           Show the version.\n"
                  "  -i, --incognito         Start with a private, in-memory window.\n"
                  "      --classic           Force the classic (Chrome 2009) interface.\n"
                  "      --modern            Force the modern dark interface.\n"
                  "      --perf-tier <tier>  auto | legacy | low | balanced | high.\n"
                  "\n"
                  "Either interface switch applies to this launch only; the saved\n"
                  "preference lives in Settings -> Appearance -> Interface style.\n",
                  stdout);
            return 0;
        }
    }

    // Must run before QApplication is constructed.
    LightSchemeHandler::registerScheme();

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icon.png"));
    QApplication::setApplicationName("Cloudar Browser");
    QApplication::setOrganizationName("Cloudar");
    QApplication::setApplicationVersion(QStringLiteral(CLOUDAR_VERSION));

    // Second pass over the performance profile: the first one ran before
    // QApplication existed, so it could only apply defaults and environment
    // overrides. Now the saved tier, hardware-acceleration and animation
    // preferences are readable and are folded into the same Chromium command
    // line before any web profile is created.
    PerformanceProfile::applyEnvironment();

    // Classic Chrome 2009 chrome vs. the modern dark one. Resolved here so the
    // very first window is built with the right metrics instead of being
    // restyled a frame later. CLOUDAR_THEME is a launch-time override, like the
    // --classic/--modern switches below; it does not rewrite the saved
    // preference.
    if (const QString forced = qEnvironmentVariable("CLOUDAR_THEME"); !forced.isEmpty()) {
        Theme::ThemeStyle parsed;
        if (Theme::styleFromKey(forced, &parsed))
            Theme::applyForRun(parsed);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Cloudar Browser - a Qt6/QtWebEngine browser with a Chrome 2009 classic chrome");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("url", "Optional URL to open on startup");

    QCommandLineOption incognitoOption({"i", "incognito"}, "Start with a private, in-memory window.");
    parser.addOption(incognitoOption);

    QCommandLineOption classicOption("classic", "Force the classic (Chrome 2009) user interface.");
    parser.addOption(classicOption);

    QCommandLineOption modernOption("modern", "Force the modern dark user interface.");
    parser.addOption(modernOption);

    QCommandLineOption perfOption(
        "perf-tier",
        "Override CPU/GPU tuning: auto, legacy, low, balanced or high.",
        "tier", "auto");
    parser.addOption(perfOption);

    parser.process(app);

    // Command-line switches are transient: they must not rewrite the saved
    // preference, or the next launch from the desktop icon would ignore the
    // user's own choice.
    if (parser.isSet(classicOption))
        Theme::applyForRun(Theme::ThemeStyle::Classic);
    else if (parser.isSet(modernOption))
        Theme::applyForRun(Theme::ThemeStyle::Modern);

    // --perf-tier applies to this launch only: it is put in the environment
    // that PerformanceProfile reads (highest priority) instead of being saved,
    // so launching from a desktop icon never overrides the user's own choice.
    if (parser.isSet(perfOption)) {
        const QString tier = parser.value(perfOption).trimmed();
        if (!tier.isEmpty() && tier.compare("auto", Qt::CaseInsensitive) != 0)
            qputenv("CLOUDAR_PERF_TIER", tier.toUtf8());
    }
    // Final environment pass, now that the effective tier and the saved
    // preferences are both readable.
    PerformanceProfile::applyEnvironment();

    // Remember the tier this launch actually applied. It is baked into the
    // Chromium command line, so a later change can only take effect on the next
    // launch and the settings page needs something to compare against.
    QSettings().setValue("performance/cpuTierAtStartup",
                         PerformanceProfile::tierKey(PerformanceProfile::tier()));

    // Ensure the profile data directory exists (history/cookies/downloads live here)
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    const bool incognito = parser.isSet(incognitoOption);
    MainWindow window(incognito);
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
