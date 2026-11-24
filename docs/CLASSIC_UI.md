# Classic Interface (Chrome 2009)

Cloudar has two complete chromes, not one skin with a color swap.

| | Classic | Modern |
|:---|:---|:---|
| Reference | Google Chrome 0.2 / 1.0 (2009) on Windows 7 | current Cloudar design |
| Window | native OS frame (Aero/Classic) | frameless, accent-coloured title bar |
| Menu | real `QMenuBar`: File / Edit / View / History / Tools / Help | custom title row + single Menu |
| Tabs | horizontal strip, grey gradient, square shoulders, 24 px | vertical sidebar (collapsible) + optional strip |
| Address bar | 23 px, square corners, page icon inside the left edge, "go" arrow when typing | rounded pill, accent focus ring, animated glow |
| Bookmarks | fixed bar below the toolbar | bar below the toolbar |
| Progress | green sliver in the status bar | progress control in the toolbar |
| Status bar | yes ("Loading…", "Done", link target) | hidden |
| AI panel / split view | not in the toolbar (reachable via **View**) | first-class toolbar buttons |

Switching is in **Settings → Appearance → Interface style** or on the command
line:

```bash
./build/cloudar-browser --classic
./build/cloudar-browser --modern
CLOUDAR_THEME=classic ./build/cloudar-browser
```

Because the window model changes (frameless vs. decorated) and the menu bar and
tab surface are rebuilt, **the style is applied at startup**. The settings page
says so and offers a restart button; the command-line switches apply
immediately for the current launch.

The three entry points differ deliberately, and the difference is a real bug
class, so it is worth stating exactly:

| Entry point | Applies now | Writes `browser/themeStyle` |
|:---|:---|:---|
| `--classic` / `--modern` | yes (`Theme::applyForRun`) | **no** |
| `CLOUDAR_THEME` | yes (`Theme::applyForRun`) | **no** |
| Settings → Interface style | on next launch (`Theme::setCurrent`) | yes |
| Tools → Appearance | on next launch (`Theme::setCurrent`) | yes |

So `./cloudar-browser --modern` gives you the modern chrome for that one run and
still starts classic tomorrow. Both menu paths show a radio group reflecting the
*saved* value, and `SettingsBridge::restartRequired()` compares the saved value
with the one `main()` recorded at startup to decide whether to show the
"restart pending" notice.

## What is faithful, and what is deliberately not

Faithful to the reference:

- the tab shape (square bottom, 3 px rounded shoulders, grey vertical gradient,
  lighter active tab that visually joins the toolbar),
- the omnibox geometry: 23 px tall, 1 px `#a0a0a0` border, 2 px radius, page
  glyph inset 3 px from the left edge,
- the toolbar order: Back, Forward, Reload, Home, separator, omnibox, star,
  reader,
- the status bar's "Loading…" / "Done" messages,
- the fact that the star is **outline** until a page is bookmarked, then solid
  yellow — the current Chrome behaviour, not the 2009 one, but more useful and
  visually identical at 16 px.

Deliberately different:

- **Menus.** Chrome 2009 had a page-control button and a wrench menu. This
  build keeps the same actions but lays them out as a Win32 menu bar, which is
  what the rest of a 2009-era Windows application looked like and what is
  actually discoverable.
- **Scrollbars.** The drawable arrow buttons of Windows Classic scrollbars are
  not reproduced; the flat 14 px `#c0c0c0` bar with a 1 px border is.
- **Icons** are painted with `QPainter` (see `Theme::icon`) rather than shipped
  as bitmaps. They are the same shapes but scale cleanly and add no binary
  weight.
- **No live wallpaper animation** in classic mode: the New Tab page still
  accepts a background image, but the drifting gradient and floating blobs are
  disabled because they cost CPU on exactly the machines this interface exists
  for.

## Implementation map

| File | Responsibility |
|:---|:---|
| `src/ClassicTheme.h` | the `ThemeStyle` enum, `Metrics` struct, icon factory |
| `src/ClassicTheme.cpp` | both QSS sheets, glyph painting, per-style metrics |
| `src/ThemeManager.cpp` | accent colour store; delegates QSS to `Theme::appStyleSheet()` |
| `src/MainWindow.cpp` | `applyChromeStyle()`, `buildClassicMenu()`, `buildClassicOmniboxDecor()` |
| `src/TitleBar.cpp` | only used by the modern chrome (classic uses the OS title bar) |
| `src/TabPreviewPopup.cpp`, `src/BrowserSidebar.cpp`, `src/BookmarkBar.cpp` | popups and strips painted in code; they read `Theme::popupColors()` / `Theme::dimTextColor()` instead of hard-coding the modern palette |

Metrics that differ between the chromes live in `Theme::metrics()` rather than
in `MainWindow` constants:

```cpp
titleBarHeight     26 (classic) / 36 (modern)
toolbarHeight      34 / 44
menuBarHeight      20 / 24
tabHeight          24 / 30
frameless          false / true
verticalTabs       false / true
statusBarVisible   true / false
```

## Adding a third chrome

1. Add a value to `ThemeStyle` and a key in `keyFor()` / `styleFromKey()`.
2. Add its QSS branch at the top of `Theme::appStyleSheet()`.
3. Fill in its `Metrics` in `Theme::metrics()`.
4. If it needs a different menu layout, branch in `MainWindow::buildMenu()`.

Nothing else should need to know the style exists.
