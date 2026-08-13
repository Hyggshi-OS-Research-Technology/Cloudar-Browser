# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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

