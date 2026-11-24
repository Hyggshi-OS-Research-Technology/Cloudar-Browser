#include "ThemeManager.h"
#include "ClassicTheme.h"

#include <QSettings>

namespace {
const char *kAccentKey = "browser/accentColor";
const char *kDefaultAccent = "#2d7ff9";
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

    // The custom TitleBar (see TitleBar.cpp) is the one place that's
    // fully accent-colored — that's the actual "recolor the title bar"
    // ask. The rest of the chrome stays a neutral dark and uses the
    // accent only for selection/hover/focus, so picking a bright accent
    // doesn't turn the whole window into a solid color block.
    const QString chromeBg = "#181b22";
    const QString chromeBorder = "#262b36";

    return QString(R"(
        QMainWindow { background: #14161c; }
        QMenuBar { background: %3; color: #e8ecf5; border: none; border-bottom: 1px solid %4;
                   padding: 2px; }
        QMenuBar::item { background: transparent; padding: 4px 10px; border-radius: 4px; }
        QMenuBar::item:selected { background: %1; color: %5; }
        QMenu { background: %3; color: #e8ecf5; border: 1px solid %4; }
        QMenu::item { padding: 5px 18px; }
        QMenu::item:selected { background: %1; color: %5; }
        QToolBar { background: %3; border: none; border-bottom: 1px solid %4; spacing: 6px; padding: 4px; }
        QToolBar QToolButton { color: #e8ecf5; border-radius: 6px; padding: 4px 8px; }
        QToolBar QToolButton:hover { background: %2; color: %5; }
        QLineEdit { background: #1d212b; color: #e8ecf5; border: 1px solid %4;
                    border-radius: 14px; padding: 6px 12px; }
        QLineEdit:focus { border-color: %1; }
        QTabWidget::pane { border: none; }
        QTabBar { background: %3; border: none; }
        QTabBar::tab { background: #1b1e28; color: #c7cede; padding: 6px 12px;
                       border-top-left-radius: 8px; border-top-right-radius: 8px;
                       margin-right: 2px; min-height: 22px; }
        QTabBar::tab:hover { background: #252a37; color: #e8ecf5; }
        QTabBar::tab:selected { background: %1; color: %5; font-weight: 500; }
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
            background-color: rgba(255, 255, 255, 0.22);
        }
        QTabBar::close-button:pressed {
            image: url(:/close-hover.svg);
            background-color: #e81123;
        }
        #NewTabStripButton { background: transparent; border: none;
                             border-radius: 6px; color: #c7cede;
                             font-size: 15px; font-weight: 600; }
        #NewTabStripButton:hover { background: rgba(255, 255, 255, 0.15); color: #ffffff; }
        #NewTabStripButton:pressed { background: rgba(255, 255, 255, 0.25); }
        #BrowserSidebar { background: #13161f; border-right: 1px solid %4; }
        #SidebarTitle { color: #e8ecf5; font-size: 14px; font-weight: 700; }
        #BrowserSidebar QToolButton { color: #e8ecf5; border-radius: 8px; padding: 6px; }
        #BrowserSidebar QToolButton:hover { background: rgba(255, 255, 255, 0.12); color: %5; }
        #BrowserSidebar QToolButton { min-width: 30px; }
        #SidebarTabs { background: transparent; border: none; outline: none; }
        #SidebarTabs::item { min-height: 38px; border-radius: 8px; margin: 2px 4px; padding: 0px; border: none; }
        #SidebarTabs::item:hover { background: #222633; }
        #SidebarTabs::item:selected { background: #2c3243; }
        #SidebarNewTabButton { background: #1c202c; border: 1px solid %4; border-radius: 8px;
                               color: #e8ecf5; font-size: 13px; font-weight: 600; padding: 8px 12px; }
        #SidebarNewTabButton:hover { background: %1; color: %5; border-color: %1; }
        #SidebarTabCloseButton { background: transparent; border: none; border-radius: 10px;
                                 color: #9aa4b8; font-size: 11px; font-weight: bold; }
        #SidebarTabCloseButton:hover { background: rgba(255, 255, 255, 0.22); color: #ffffff; }
        #SidebarTabCloseButton:pressed { background: #e81123; color: #ffffff; }
        QProgressBar { background: transparent; border: none; }
        QProgressBar::chunk { background: %1; border-radius: 2px; }
        QStatusBar { background: %3; border: none; }
    )").arg(accent, accentHover, chromeBg, chromeBorder, text);
}
