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
    // changing it in system settings). Qt 6.5 is the first release with
    // QStyleHints::colorScheme()/colorSchemeChanged() and Qt::ColorScheme at
    // all, and the distro-Qt CI jobs build against older releases — so the whole
    // block is version-gated, not just the call. Without this the app does not
    // compile there at all.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
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
#endif
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

// Two full palettes, not a tint of one. A light surface needs different values
// for every role (window, chrome, border, tab, hover, input) — a single dark
// sheet with a light window background reads as a broken dark theme, which is
// exactly the "style applied halfway" defect this project treats as a bug.
//
// These are the *only* light/dark values in the program. The stylesheet below
// and the widgets that paint themselves (tab close buttons, the title bar,
// painted popups) both read them, so a role cannot be right in one place and
// wrong in the other — which is how the tab close button ended up dark-only.
ThemeManager::Palette ThemeManager::paletteFor(bool dark)
{
    Palette p;
    if (dark) {
        p.window   = QColor("#14161c");
        p.chrome   = QColor("#181b22");
        p.border   = QColor("#262b36");
        p.tab      = QColor("#1b1e28");
        p.tabHover = QColor("#252a37");
        p.hover    = QColor("#222633");
        p.input    = QColor("#1d212b");
        p.text     = QColor("#e8ecf5");
        p.dim      = QColor("#c7cede");
    } else {
        p.window   = QColor("#f7f8fa");
        p.chrome   = QColor("#ffffff");
        p.border   = QColor("#d6dae2");
        p.tab      = QColor("#f1f2f5");
        p.tabHover = QColor("#e7e9ee");
        p.hover    = QColor("#eceef3");
        p.input    = QColor("#ffffff");
        p.text     = QColor("#1a1d24");
        p.dim      = QColor("#5b6273");
    }
    // Filled in by the caller: it depends on the accent, not the mode.
    p.onAccent = QColor("#0b0e14");
    return p;
}

ThemeManager::Palette ThemeManager::palette() const
{
    Palette p = paletteFor(prefersDark());
    p.onAccent = readableTextOn(m_accent);
    return p;
}

QString ThemeManager::modernStyleSheet(const QColor &color) const
{
    const QColor accentColor = color.isValid() ? color : m_accent;
    const QString accent = accentColor.name();
    const QString accentHover = accentColor.lighter(115).name();

    const Palette pal = paletteFor(prefersDark());
    const QString onAccent = ThemeManager::readableTextOn(accentColor).name();
    // Local names for the roles the sheet below interpolates. Kept as
    // #rrggbb strings because that is what QSS parses.
    const auto hex = [](const QColor &c) { return c.name(); };
    const QString pWindow = hex(pal.window), pChrome = hex(pal.chrome),
                  pBorder = hex(pal.border), pTab = hex(pal.tab),
                  pTabHover = hex(pal.tabHover), pHover = hex(pal.hover),
                  pInput = hex(pal.input), pText = hex(pal.text), pDim = hex(pal.dim);

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
        /* The close glyph is painted (Theme::icon("close")), not an SVG: the
           shipped close.svg is a fixed #c7cede cross, which is white on the
           light tab surface and therefore invisible in light mode. A QSS
           `image:` cannot follow the palette, so the *shape* comes from
           qproperty-less painting and only the wash comes from QSS. */
        QTabBar::tab:selected QTabBar::close-button, QTabBar::tab:hover QTabBar::close-button {
            subcontrol-position: right;
            margin-left: 4px;
            margin-right: 2px;
            padding: 3px;
            border-radius: 8px;
        }
        QTabBar::close-button:hover {
            background-color: rgba(127, 127, 127, 0.28);
        }
        QTabBar::close-button:pressed {
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
        #SidebarDropIndicator { background: %1; border-radius: 1px; }
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

        /* ---- Dialogs and their controls ----
           The sheet stopped at the main window, so every dialog this app opens
           (message boxes, the file picker's Qt-side widgets, Extensions,
           Settings, prompts) was drawn by the platform style *inside* a dark
           window: a white QMessageBox, black-on-black QPushButton labels under
           a dark sheet that does set QWidget colour, and a QLineEdit whose
           only rule was the pill shape meant for the omnibox. These rules are
           the missing half of the palette. */
        QDialog { background: %3; }
        QDialog QLabel, QDialog QCheckBox, QDialog QRadioButton { color: %10; }
        QMessageBox { background: %4; }
        QMessageBox QLabel { color: %10; }
        QPushButton { background: %9; color: %10; border: 1px solid %5;
                      border-radius: 6px; padding: 5px 16px; min-width: 64px; }
        QPushButton:hover { background: %2; color: %6; border-color: %1; }
        QPushButton:pressed { background: %1; color: %6; }
        QPushButton:default { border-color: %1; }
        QPushButton:disabled { color: %11; border-color: %5; background: %3; }
        /* Not the omnibox pill: an input inside a dialog is a plain field. */
        QDialog QLineEdit, QDialog QSpinBox, QDialog QComboBox {
            background: %8; color: %10; border: 1px solid %5;
            border-radius: 6px; padding: 4px 8px;
        }
        QDialog QLineEdit:focus { border-color: %1; }
        QComboBox QAbstractItemView { background: %4; color: %10;
                                      border: 1px solid %5;
                                      selection-background-color: %1;
                                      selection-color: %6; }
        QListWidget, QTreeWidget, QTableWidget, QTextBrowser, QPlainTextEdit {
            background: %8; color: %10; border: 1px solid %5; border-radius: 6px;
        }
        QListWidget::item:selected, QTreeWidget::item:selected,
        QTableWidget::item:selected { background: %1; color: %6; }
        QListWidget::item:hover { background: %9; }
        QTabWidget::pane { border: 1px solid %5; border-radius: 6px; }
        QGroupBox { color: %10; border: 1px solid %5; border-radius: 6px;
                    margin-top: 10px; padding-top: 6px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; color: %11; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
        QScrollBar::handle:vertical { background: %12; min-height: 28px;
                                      border-radius: 5px; }
        QScrollBar::handle:vertical:hover { background: %11; }
        QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
        QScrollBar::handle:horizontal { background: %12; min-width: 28px;
                                        border-radius: 5px; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QToolTip { background: %4; color: %10; border: 1px solid %5; padding: 4px 6px; }
    )").arg(accent)                                 // 1
       .arg(accentHover)                             // 2
       .arg(pWindow)                                 // 3
       .arg(pChrome)                                 // 4
       .arg(pBorder)                                 // 5
       .arg(onAccent)                                // 6
       .arg(pTab)                                    // 7
       .arg(pInput)                                  // 8
       .arg(pTabHover)                               // 9
       .arg(pText)                                   // 10
       .arg(pDim)                                    // 11
       .arg(pHover);                                 // 12
}
