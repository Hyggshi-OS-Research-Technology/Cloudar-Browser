#pragma once

#include <QString>
#include <QColor>
#include <QIcon>
#include <QStyle>

// ---------------------------------------------------------------------------
// Visual themes.
//
// Cloudar ships two complete chromes:
//
//   Modern   - the dark, rounded, translucent chrome the app grew up with.
//   Classic  - a faithful reconstruction of Google Chrome 0.2/1.0 on 2009
//              Windows: 23 px address bar with a blue "page" icon, a yellow
//              star, a "go" arrow, a bookmarks bar on a beige Windows frame,
//              grey gradient tabs with square shoulders, and a status bar.
//
// Both are pure QSS + a handful of QPainter-drawn icons (see
// ClassicChrome.cpp), so switching is instant and costs no extra memory.
// The old chrome is *not* a skin on top of the new one: layout sizes,
// paddings, fonts and icon sets all differ, which is why MainWindow asks
// this class for metrics instead of hard-coding them.
// ---------------------------------------------------------------------------

namespace Theme {

// Lives inside the namespace so callers write Theme::ThemeStyle, which is what
// every use site already does. A global enum plus a namespace of functions is
// the one combination that reads inconsistently and, as it turned out here,
// silently fails to compile at every call site.
enum class ThemeStyle {
    Modern,
    Classic
};

// The style in force for *this process*, persisted at "browser/themeStyle"
// ("classic" or "modern").
//
// current() and setCurrent() do not read and write QSettings unconditionally:
// an interface style decides the window model, so a launch-time switch
// (--classic, --modern, CLOUDAR_THEME) must apply for that run without
// rewriting the user's saved preference, while a change made in the UI is
// saved for the next launch. The pair below separates the two:
//
//   current()          - the style in force now
//   savedStyle()       - the persisted preference (what the settings UI shows)
//   setCurrent(style)  - save only; takes effect on the next launch
//   applyForRun(style) - switch now, do not save (the command-line path)
ThemeStyle current();
// The persisted preference, which may differ from current() when the launch
// used --classic / --modern / CLOUDAR_THEME. Settings UI reads this one: the
// interface-style control is about what happens next launch.
ThemeStyle savedStyle();
void setCurrent(ThemeStyle style);
void applyForRun(ThemeStyle style);
QString keyFor(ThemeStyle style);
bool styleFromKey(const QString &key, ThemeStyle *out);

// There is no signal here: MainWindow re-reads current() whenever it rebuilds
// its stylesheet, and an interface-style change only takes effect on the next
// launch anyway (see restartRequired()).

// Full application QSS for the active style.
QString appStyleSheet(const QColor &accent);

// Ui metrics used by MainWindow/BrowserTabWidget so the two chromes can
// differ in height without scattering magic numbers.
struct Metrics {
    int titleBarHeight;
    int toolbarHeight;
    int menuBarHeight;
    int tabHeight;
    int sidebarCollapsedWidth;
    int sidebarExpandedWidth;
    bool frameless;      // Classic uses the OS window frame
    bool verticalTabs;   // Modern defaults to the sidebar, Classic to a strip
    bool statusBarVisible;
    bool bookmarkBarVisible;
};

Metrics metrics();

// Small painted icons (no image assets needed, so they scale cleanly and
// cost nothing at build time).
QIcon icon(const QString &name, int size = 16);
// Dark/light variants for the classic chrome's grey vs. blue toolbars.
QColor textColor();
QColor dimTextColor();
QColor accentTextColor(const QColor &accent);

// Colours for the small popups that are painted in code rather than by the
// stylesheet (tab hover cards, sidebar previews). They float above everything
// else, so they cannot inherit the chrome's palette: in the classic chrome a
// dark hover card would be the only black rectangle on a light grey window.
struct PopupColors {
    QColor surface;
    QColor border;
    QColor text;
    QColor dim;
    QColor placeholder;
};
PopupColors popupColors();

} // namespace Theme
