# Changelog

All notable changes to Cloudar Browser. Version numbers follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- **Web notifications** (`src/NotificationPresenter.*`, `src/NotificationToast.*`). QtWebEngine
  has had `QWebEngineProfile::setNotificationPresenter()` since Qt 5.13, so this is a real
  implementation: an allowed site's notification becomes a card in the corner of the window that
  owns the page, clicking it runs the page's `click()` handler and raises the tab, dismissing it
  runs `close()`. At most three, oldest evicted, and a denied site is dropped silently.
  - **Not OS notifications**: nothing is delivered while the window is closed and there is no
    notification centre. The permission prompt says so before the user allows it, and
    `docs/KNOWN_ISSUES.md` §12 lists the rest.
- **Web shortcuts / app windows** (`src/WebShortcutStore.*`, `src/WebShortcutDialog.*`,
  `MainWindow::createAppWindow`). A saved page that opens in its own window: no tab strip, no
  omnibox, no menus, its own title and remembered geometry. Reached from the classic **Tools**
  menu and the modern ☰ menu, and from `--app <url>` / `--app-id <id>`.
  - Explicitly **not** a PWA install: no manifest, no service worker, no offline cache, no
    launcher entry. See `docs/KNOWN_ISSUES.md` §13.
- `tests/tst_webshortcutstore.cpp`, and the CI step that runs it.

### Changed

- The modern chrome now has one palette definition (`ThemeManager::paletteFor()`), used by both
  the stylesheet and every widget that paints itself. Previously the painted widgets hard-coded
  the dark values.
- New gate: `tools/check-css.js` verifies that both chromes' stylesheet substitutions are
  complete (a role added to the sheet but not to the `.arg()` chain renders as a literal `%13`
  that Qt silently drops) and that the modern sheet loads no image assets with baked-in colours.

### Fixed

- **Light mode drew dark-only chrome.** The tab close button was a fixed `#c7cede` cross — white
  on the light tab surface, i.e. an invisible button — the tab hover card was a near-black
  rectangle over a white window, the title bar's window controls kept dark-mode glyphs after a
  light/dark flip, and the sidebar's row dividers used a `rgba(255,255,255,0.08)` hairline that
  does not exist on a light background. All four now derive from the shared palette; the close
  glyph is painted (`Theme::icon("close")`) instead of shipped as an SVG with a baked-in colour.
- **The modern stylesheet had no rules for dialogs.** Every `QMessageBox`, `QFileDialog` and
  prompt in a dark window was drawn by the platform style — a white box, or black-on-black button
  labels under a sheet that does set `QWidget` colour. Dialogs, buttons, inputs, lists, group
  boxes, scrollbars and tooltips now have palette rules in both modes.
- **A notification permission prompt claimed more than the browser does.** It now carries its own
  explanatory sentence (`permission_info_notifications`) instead of the generic
  "you can change this later".

## [0.2.0] — 2026-11-24

The "classic chrome + honest hardware support" release.

### Added

- **Classic interface (Chrome 2009).** A second, complete chrome modelled on
  Google Chrome 0.2/1.0: native window frame, Win32-style menu bar, grey
  gradient tab strip with square shoulders, 23 px omnibox with an inset page
  icon and a "go" arrow, Home/Reload/Stop buttons, bookmark bar, and a status
  bar with "Loading…/Done" plus a green progress sliver. Toolbar glyphs are
  painted with `QPainter` (`src/ClassicTheme.cpp`) rather than shipped as
  bitmaps.
  - Switch with **Settings → Appearance → Interface style**, `--classic` /
    `--modern`, or `CLOUDAR_THEME`.
  - The settings, New Tab and About pages reskin themselves to match.
  - Documented in `docs/CLASSIC_UI.md`.
- **CPU compatibility tiers** (`src/PerformanceProfile.*`): CPUID/RAM detection
  before `QApplication` construction chooses Legacy / Low / Balanced / High and
  sets Chromium's raster threads, renderer-process limit, GPU usage and Qt's
  rendering backend accordingly. Legacy also disables UI animation.
  - Overrides: `--perf-tier=<tier>`, `CLOUDAR_PERF_TIER`, or
    **Settings → Performance → Performance tier**.
  - Documented in `docs/CPU_COMPATIBILITY.md`.
- **Real Memory Saver** (`src/MemorySaver.*`). The Light/Medium/High/Ultra
  levels now actually discard idle background renderers, replace them with a
  lightweight "sleeping" placeholder, and rebuild the page from the remembered
  URL when the tab is activated. Sleeping tabs survive session save/restore.
- **Performance panel** in `cloudar://settings` showing detected hardware,
  renderer mode, effective Chromium switches and live sleeping-tab statistics.
- **Honest About panel**: Qt version, Chromium version, platform, interface
  style and performance tier are all reported from the running binary.
- **CI matrix** covering Ubuntu 22.04/24.04 with distro Qt, Qt 6.8 LTS and Qt
  6.10 from the official installer, plus Windows Server 2022/2025 with Qt 6.8
  LTS and 6.10; classic and modern headless smoke tests on every Linux job.
- Documentation: `docs/PLATFORM_MATRIX.md`, `docs/CPU_COMPATIBILITY.md`,
  `docs/CLASSIC_UI.md`, `docs/KNOWN_ISSUES.md`, `CHANGELOG.md`.

### Changed

- The frameless window is now a property of the *modern* chrome only; the
  classic chrome uses the platform's decorated window, which is what Chrome
  2009 did.
- `Theme::metrics()` is the single source of truth for chrome dimensions
  (title bar, toolbar, menu, tab height, sidebar widths, status bar).
- `ThemeManager` delegates stylesheet generation to `Theme::appStyleSheet()` so
  the modern sheet is no longer duplicated.
- `SettingsBridge::appVersion()` reads the application version instead of
  hard-coding `0.1.0`.
- The address-bar glow, tab fade-in and progress tween are skipped when
  animations are disabled or the classic chrome is active.
- `--classic`, `--modern` and `CLOUDAR_THEME` no longer rewrite the saved
  interface-style preference; only the Settings switch and the menu do.
- Changing the interface style is now honestly *save-only*: it no longer
  repaints the running window with the other chrome's stylesheet while keeping
  the old layout. `Tools ▸ Appearance` and the settings page both say the change
  lands on the next launch, and `Tools ▸ Restart` is one click away.
- `SettingsBridge::uiStyle()` and both Appearance menus report the *saved*
  preference, so a one-off `--modern` launch no longer looks like a permanent
  choice, and `restartRequired()` compares the running chrome against the saved
  one.
- The performance profile is applied twice at startup (before and after
  `QApplication`), so the *saved* tier and hardware-acceleration preference now
  reach Chromium's command line instead of always falling back to autodetection.
  Any pre-existing `QTWEBENGINE_CHROMIUM_FLAGS` are preserved rather than
  overwritten, and repeated calls no longer duplicate switches.
- Split view toggling moved into `MainWindow::toggleSplitView()` so both chromes
  and the View menu share one code path.

### Fixed

- **Fabricated per-tab memory figures removed.** The sidebar and tab-preview
  hover cards printed `120 + (qHash(url) % 65) MB` labelled "Memory usage";
  they now report the tab's real state (Active / Background / Sleeping) and no
  invented numbers.
- **Fabricated "Clean Memory Now" result removed.** It returned
  `145 + rand() % 40` MB. It now discards background renderers, clears the HTTP
  cache and returns what it actually freed (an estimate where measurement is
  impossible, labelled as such).
- Tab hover previews and the vertical sidebar no longer lose a tab's title and
  URL when that tab is sleeping.
- `MainWindow::setSidebarVisible()` cannot leave the browser with no visible tab
  surface in the classic chrome.
- Progress reporting no longer dereferences a spinner that the classic chrome
  does not create.
- **`SettingsBridge::restartRequired()`, `setShowHomeButton()` and
  `setShowBookmarkBar()` are now defined.** They were declared and called from
  the settings page but missing from the implementation file, which would have
  failed to link.
- **A duplicated `SettingsBridge` implementation block was removed**; it
  defined 40+ slots twice and would also have failed to link.
- The Home button is always created and only its visibility follows the
  setting, so toggling it on in Settings no longer does nothing until a restart.
- Switching chrome now clears the old icon set as well as applying the new one,
  so a classic glyph and a modern glyph are never drawn side by side.
- The classic omnibox's page icon and "go" arrow are hidden again when the
  modern chrome is active, and the "go" arrow only appears in the classic
  chrome.
- `MemorySaver`'s "released memory" figure is derived from the live sleeping-tab
  count instead of accumulating forever; a woken tab stops counting as saved.
- `Theme::current()`/`setCurrent()` keep an in-process value, so a style chosen
  on the command line applies for that run and the menu check state reflects the
  *saved* preference rather than the running one.
- `LocalContentCanAccessFileUrls` and `AllowRunningInsecureContent` were enabled
  for every profile; both are now off (the New Tab page's remote wallpaper still
  works through `LocalContentCanAccessRemoteUrls`).
- The New Tab page's tiles fall back to their `cloudar://` target when the
  native bridge is unavailable, instead of doing nothing.
- CI's headless smoke tests now fail on any exit other than the timeout, so a
  build that starts and immediately dies is caught instead of passing.
- The Windows artifact no longer ships the Linux-only `run_cpp.sh`.
- Tab hover cards and the sidebar card use per-chrome colours
  (`Theme::popupColors()`); the classic chrome no longer paints a dark modern
  card over a light window.
- The bookmarks-bar empty hint uses the active chrome's dim text colour instead
  of 35%-transparent white, which was invisible on the classic beige strip.

### Known limitations

See `docs/KNOWN_ISSUES.md`. Summary: Windows 7/8/8.1 and Android are out of
scope for this branch (Qt 6 cannot target them), extension support is a partial
`chrome.*` shim, and incognito is not anonymity.

---

## [0.1.0]

- Initial C++17/Qt6 rewrite of the Python prototype: tabbed browsing, vertical
  sidebar, internal `cloudar://` pages, AdBlock, AI panel, split view, reader
  mode, session restore, `.crx` loading, accent-colour theming.
