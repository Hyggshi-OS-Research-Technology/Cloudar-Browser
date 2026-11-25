#include "ThemeManager.h"
#include "ClassicTheme.h"

#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>
#include <QtGlobal>

namespace {
const char *kAccentKey = "browser/accentColor";
const char *kDefaultAccent = "#2d7ff9";
const char *kColorModeKey = "browser/colorMode";
}

ThemeManager &ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent)
{
    QSettings settings;
    m_accent = QColor(settings.value(kAccentKey, kDefaultAccent).toString());
    if (!m_accent.isValid())
        m_accent = QColor(kDefaultAccent);

    const QString mode = settings.value(kColorModeKey, "system").toString().trimmed().toLower();
    if (mode == "light")
        m_mode = ColorMode::Light;
    else if (mode == "dark")
        m_mode = ColorMode::Dark;
    else
        m_mode = ColorMode::System;
    m_lastDark = prefersDark();

    // The OS can flip light/dark while the app runs (a schedule, or the user
    // changing it in system settings). Qt 6.5 exposes that as a signal; older
    // builds have no way to observe it, which
    // systemColorSchemeQueryable() reports so the UI can stop pretending.
    if (QGuiApplication::instance() != nullptr) {
        const QStyleHints *hints = QGuiApplication::styleHints();
        if (hints) {
            connect(hints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
                const bool dark = prefersDark();
                if (dark == m_lastDark)
                    return;
                m_lastDark = dark;
                emit colorModeChanged(dark);
            });
        }
    }
}

bool ThemeManager::systemColorSchemeQueryable()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints() != nullptr;
#else
    // No public API. Reporting false is the honest answer: the settings page
    // uses it to explain that "System" means light on this build.
    return false;
#endif
}

bool ThemeManager::systemPrefersDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const QStyleHints *hints = QGuiApplication::styleHints();
    return hints && hints->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

bool ThemeManager::prefersDark() const
{
    switch (m_mode) {
    case ColorMode::Light: return false;
    case ColorMode::Dark:  return true;
    case ColorMode::System: break;
    }
    return systemPrefersDark();
}

ColorMode ThemeManager::colorMode() const
{
    return m_mode;
}

QString ThemeManager::colorModeKey() const
{
    switch (m_mode) {
    case ColorMode::Light: return QStringLiteral("light");
    case ColorMode::Dark:  return QStringLiteral("dark");
    case ColorMode::System: break;
    }
    return QStringLiteral("system");
}

void ThemeManager::setColorMode(ColorMode mode)
{
    if (mode == m_mode)
        return;
    m_mode = mode;
    QSettings().setValue(kColorModeKey, colorModeKey());

    const bool dark = prefersDark();
    if (dark != m_lastDark) {
        m_lastDark = dark;
        emit colorModeChanged(dark);
    }
}

QColor ThemeManager::accentColor() const
{
    return m_accent;
}

void ThemeManager::setAccentColor(const QColor &color)
{
    if (!color.isValid() || color == m_accent)
        return;

    m_accent = color;

    QSettings settings;
    settings.setValue(kAccentKey, color.name());

    emit accentChanged(m_accent);
}

QColor ThemeManager::readableTextOn(const QColor &bg)
{
    // Standard perceived-luminance check; flips to dark text once the
    // accent itself gets light enough that white text would wash out.
    const double luminance = (0.299 * bg.red() + 0.587 * bg.green() + 0.114 * bg.blue()) / 255.0;
    return luminance > 0.6 ? QColor("#10131a") : QColor("#f5f7fb");
}

QString ThemeManager::appStyleSheet() const
{
    return Theme::appStyleSheet(m_accent);
}

QString ThemeManager::modernStyleSheet(const QColor &color) const
{
    const QColor accentColor = color.isValid() ? color : m_accent;
    const QString accent = accentColor.name();
    const QString accentHover = accentColor.lighter(115).name();
    const QString text = readableTextOn(accentColor).name();

    // Two full palettes, not a tint of one. A light surface needs different
    // values for every role (window, chrome, border, tab, hover, input) — a
    // single dark sheet with a light window background reads as a broken dark
    // theme, which is exactly the "style applied halfway" defect this project
    // treats as a bug.
    struct Palette {
        const char *window;   // behind the whole window
        const char *chrome;   // menu bar, toolbar, tab strip, status bar
        const char *border;   // 1px separators and outlines
        const char *tab;      // unselected tab
        const char *tabHover;
        const char *hover;    // generic hover surface
        const char *input;    // address bar
        const char *text;     // primary text
        const char *dim;      // secondary text and unselected tab labels
        const char *strong;   // text on the accent
    };
    static const Palette kDark = {
        "#14161c", "#181b22", "#262b36", "#1b1e28", "#252a37",
        "#222633", "#1d212b", "#e8ecf5", "#c7cede", "#0b0e14"
    };
    static const Palette kLight = {
        "#f7f8fa", "#ffffff", "#d6dae2", "#f1f2f5", "#e7e9ee",
        "#eceef3", "#ffffff", "#1a1d24", "#5b6273", "#0b0e14"
    };
    const Palette &p = prefersDark() ? kDark : kLight;

    // The accent is the one colour that stays as the user chose it; only the
    // text drawn on top of it flips.
    const QString onAccent = ThemeManager::readableTextOn(accentColor).name();

    return QString(R"(
        QMainWindow { background: %3; }
        QMenuBar { background: %4; color: %10; border: none; border-bottom: 1px solid %5;
                   padding: 2px; }
        QMenuBar::item { background: transparent; padding: 4px 10px; border-radius: 4px; }
        QMenuBar::item:selected { background: %1; color: %6; }
        QMenu { background: %4; color: %10; border: 1px solid %5; }
        QMenu::item { padding: 5px 18px; }
        QMenu::item:selected { background: %1; color: %6; }
        QToolBar { background: %4; border: none; border-bottom: 1px solid %5; spacing: 6px; padding: 4px; }
        QToolBar QToolButton { color: %10; border-radius: 6px; padding: 4px 8px; }
        QToolBar QToolButton:hover { background: %2; color: %6; }
        QLineEdit { background: %8; color: %10; border: 1px solid %5;
                    border-radius: 14px; padding: 6px 12px; }
        QLineEdit:focus { border-color: %1; }
        QTabWidget::pane { border: none; }
        QTabBar { background: %4; border: none; }
        QTabBar::tab { background: %7; color: %11; padding: 6px 12px;
                       border-top-left-radius: 8px; border-top-right-radius: 8px;
                       margin-right: 2px; min-height: 22px; }
        QTabBar::tab:hover { background: %9; color: %10; }
        QTabBar::tab:selected { background: %1; color: %6; font-weight: 500; }
        QTabBar::tab:!selected:!hover QTabBar::close-button {
            image: none;
        }
        QTabBar::tab:selected QTabBar::close-button, QTabBar::tab:hover QTabBar::close-button {
            image: url(:/close.svg);
            subcontrol-position: right;
            margin-left: 4px;
            margin-right: 2px;
            padding: 3px;
            border-radius: 8px;
        }
        QTabBar::close-button:hover {
            image: url(:/close-hover.svg);
            background-color: rgba(127, 127, 127, 0.28);
        }
        QTabBar::close-button:pressed {
            image: url(:/close-hover.svg);
            background-color: #e81123;
        }
        #NewTabStripButton { background: transparent; border: none;
                             border-radius: 6px; color: %11;
                             font-size: 15px; font-weight: 600; }
        #NewTabStripButton:hover { background: %9; color: %10; }
        #NewTabStripButton:pressed { background: %2; }
        #BrowserSidebar { background: %3; border-right: 1px solid %5; }
        #SidebarTitle { color: %10; font-size: 14px; font-weight: 700; }
        #BrowserSidebar QToolButton { color: %10; border-radius: 8px; padding: 6px; }
        #BrowserSidebar QToolButton:hover { background: %9; color: %6; }
        #BrowserSidebar QToolButton { min-width: 30px; }
        #SidebarTabs { background: transparent; border: none; outline: none; }
        #SidebarTabs::item { min-height: 38px; border-radius: 8px; margin: 2px 4px; padding: 0px; border: none; }
        #SidebarTabs::item:hover { background: %12; }
        #SidebarTabs::item:selected { background: %9; }
        #SidebarNewTabButton { background: %8; border: 1px solid %5; border-radius: 8px;
                               color: %10; font-size: 13px; font-weight: 600; padding: 8px 12px; }
        #SidebarNewTabButton:hover { background: %1; color: %6; border-color: %1; }
        #SidebarTabCloseButton { background: transparent; border: none; border-radius: 10px;
                                 color: %11; font-size: 11px; font-weight: bold; }
        #SidebarTabCloseButton:hover { background: rgba(127, 127, 127, 0.3); color: %10; }
        #SidebarTabCloseButton:pressed { background: #e81123; color: #ffffff; }
        QProgressBar { background: transparent; border: none; }
        QProgressBar::chunk { background: %1; border-radius: 2px; }
        QStatusBar { background: %4; color: %10; border: none; }
    )").arg(accent,                                  // 1
            accentHover,                             // 2
            QString::fromLatin1(p.window),           // 3
            QString::fromLatin1(p.chrome),            // 4
            QString::fromLatin1(p.border),            // 5
            onAccent,                                 // 6
            QString::fromLatin1(p.tab),               // 7
            QString::fromLatin1(p.input),             // 8
            QString::fromLatin1(p.tabHover),          // 9
            QString::fromLatin1(p.text),              // 10
            QString::fromLatin1(p.dim),               // 11
            QString::fromLatin1(p.hover));            // 12
}
