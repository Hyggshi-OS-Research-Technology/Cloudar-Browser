# AGENTS.md

This file provides guidance to coding agents working in this repository.

> **Read this first: the repository contains two implementations.**
>
> | Path | Stack | Status |
> |:---|:---|:---|
> | `src/`, `resources/`, `CMakeLists.txt` | C++17 + Qt 6 Widgets + QtWebEngine | **current / maintained** |
> | `legacy/` | Python + PyQt6 prototype | frozen, archived reference only |
>
> Everything below the line "Legacy Python prototype" describes `legacy/` and
> must not be used to reason about the native tree. Two rules follow from this:
> **do not "fix" the C++ browser by editing Python**, and **do not port a Python
> module's structure into `src/`** — the native code is organised differently on
> purpose (see `docs/README.md` for the real map).

## The native tree (C++17 / Qt 6)

### Build

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/cloudar-browser --classic      # or --modern
```

CI (`.github/workflows/build.yml`) builds on Ubuntu 22.04/24.04 and Windows
Server 2022/2025 across Qt 6.8 LTS and 6.10, then runs headless smoke tests in
both chromes.

### Architecture in one page

- `src/main.cpp` — sets the CPU tier and interface style **before**
  `QApplication` exists, parses `--classic/--modern/--perf-tier/-i`, then opens
  `MainWindow`.
- `src/PerformanceProfile.*` — CPUID/RAM detection and Chromium switch policy.
  Do not add `--no-sandbox` or `QTWEBENGINE_DISABLE_SANDBOX` here or anywhere
  else; the sandbox is not a performance knob.
- `src/ClassicTheme.*` — the two chrome stylesheets, per-chrome metrics
  (`Theme::metrics()`), and the painted toolbar icons. New UI code should ask
  these for sizes and colours instead of hard-coding either chrome's values.
- `src/MainWindow.*` — the window, both menu layouts, the omnibox decoration,
  and `applyChromeStyle()`.
- `src/MemorySaver.*` + `BrowserTabWidget::sleepTab()/reopenTabAt()` — real
  idle-tab discarding. Any new statistic shown to the user must be measured,
  not derived from a hash; see `docs/KNOWN_ISSUES.md` §6 for the history here.
- `src/SettingsBridge.*` — `cloudar://settings` backend, exposed over
  QWebChannel. Setters either apply live or are reported by
  `restartRequired()`; they never pretend.

### Conventions

- **Code comments and commit messages in English.** User-facing strings are
  English too (the `lang/` JSON files are a legacy artefact of the Python tree).
- Never fabricate a measurement, a version string or a platform capability. If
  something is unimplemented, it goes in `docs/KNOWN_ISSUES.md`.
- Qt version guards: `#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)` around
  `QWebEnginePermission`; the older `featurePermissionRequested` path must keep
  compiling (Qt 6.5 LTS is a supported target).
- New source files must be added to **both** the `SOURCES` and `HEADERS` lists in
  `CMakeLists.txt`.

### Verification you can run without a display

```bash
./build/cloudar-browser --version                       # exercises tier + theme + CLI
CLOUDAR_PERF_TIER=legacy ./build/cloudar-browser --version
CLOUDAR_THEME=modern ./build/cloudar-browser --version
```

There is no unit-test target yet; adding Qt Test coverage for
`PerformanceProfile`, `MemorySaver` and `Theme::metrics()` is the highest-value
contribution (see `docs/KNOWN_ISSUES.md` §9).

---

# Legacy Python prototype (`legacy/`) — archived

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project Overview

Hyggshi Browser (also called Cloudar Browser™) is a desktop web browser built with **PyQt6** and **QtWebEngine** (Chromium-based). It supports tabbed browsing, bookmarks, history, downloads, extensions, incognito mode, and AI sidebar integration.

## Commands

```bash
# Install dependencies
pip install PyQt6 PyQt6-WebEngine

# Run the browser
python main.py

# Build desktop executable (Windows)
python build_wrapper.py          # or run Build_app.bat

# Build Android APK (via BeeWare Briefcase)
pip install briefcase
python build_wrapper.py android
```

No test suite or linting configuration is present in this repository.

## Architecture

### Compatibility Layer
- `browser_qt.py` â€” Central compatibility shim that imports from PyQt6 (preferred) or falls back to PyQt5. All other modules import Qt classes from here, never directly from PyQt6/PyQt5. Sets `QtVersion = 6` or `5`.

### Entry Point
- `main.py` â€” Registers the custom `cloudar://` URL scheme, creates `QApplication`, and launches `BrowserWindow`. Detects Android at runtime and delegates to `mobile_browser.py`.

### Core Modules
- `browser_window.py` â€” Main `QMainWindow` subclass. Orchestrates all managers, sets up toolbars, tabs, menus, and keyboard shortcuts. This is the largest file and the central hub of the application.
- `web_view.py` â€” Custom `QWebEngineView` subclass for individual tab views.
- `tab_widget.py` â€” Custom `QTabWidget` with close buttons and tab management.

### Feature Managers
- `bookmark_manager.py` â€” Save/load bookmarks from `browser_data/bookmarks.json`.
- `history_manager.py` â€” Track and display browsing history from `browser_data/history.json`.
- `download_manager.py` â€” Handle downloads, track progress, store records in `browser_data/downloads.json`.
- `session_manager.py` â€” `BrowserSession` (persistent) and `IncognitoSession` (ephemeral) classes that manage per-session QWebEngineProfile, history, and downloads.
- `extension_manager.py` â€” Load user scripts from the `extensions/` directory.
- `settings_backend.py` + `settings_dialog.py` â€” Settings persistence (via `QWebChannel` bridge for internal pages) and the settings dialog UI.
- `performance_manager.py` â€” Memory saver, energy saver, hardware acceleration toggles.
- `internal_handler.py` â€” Handles `cloudar://` custom URL scheme, serving HTML from `internal_pages/`.

### UI & Styling
- `styles.py` â€” Qt stylesheet (QSS) for the dark theme.
- `find_bar.py` â€” In-page text search bar.
- `media_control.py` â€” Media playback control popup.
- `ai_sidebar.py` â€” AI assistant sidebar panel.

### Static Pages & Resources
- `internal_pages/` â€” HTML pages served via `cloudar://` scheme: `newtab.html`, `about.html`, `settings.html`, `extensions.html`.
- `resources/` â€” Application icon files (`Icon.ico`, `Icon.png`).

### Data Storage
- `browser_data/` â€” Runtime data directory (JSON files for bookmarks, history, settings, downloads). Created automatically on startup.

### Build & Packaging
- `build_wrapper.py` â€” Build script wrapping PyInstaller and Briefcase.
- `CloudarBrowser.spec` / `HyggshiBrowser.spec` â€” PyInstaller spec files for desktop executables.
- `pyproject.toml` â€” BeeWare Briefcase configuration for Android packaging.

