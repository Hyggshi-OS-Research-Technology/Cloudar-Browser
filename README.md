# 🌐 Cloudar Browser™

<p align="center">
  <img src="resources/icon.png" width="128" height="128" alt="Cloudar Browser Icon" />
</p>

<p align="center">
  <strong>A native web browser in C++17 and Qt6 (QtWebEngine / Chromium) — with two complete
  interfaces: a faithful reconstruction of Google Chrome 2009 and a modern dark chrome.</strong>
</p>

<p align="center">
  <a href="https://github.com/Hyggshi-OS-Research-Technology/Cloudar-Browser/actions"><img src="https://img.shields.io/github/actions/workflow/status/Hyggshi-OS-Research-Technology/Cloudar-Browser/build.yml?branch=main&label=CI%20Build&logo=github" alt="CI Status" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg?logo=c%2B%2B" alt="C++17" />
  <img src="https://img.shields.io/badge/Qt-6.5%20LTS%20--%206.10+-41CD52.svg?logo=qt" alt="Qt6" />
  <img src="https://img.shields.io/badge/Engine-QtWebEngine%20(Chromium)-orange.svg" alt="QtWebEngine" />
  <img src="https://img.shields.io/badge/Platform-Windows%2010%2F11%20%C2%B7%20Linux%20x86__64-yellow.svg" alt="Platform" />
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License" />
</p>

---

## 📖 Overview

**Cloudar Browser™** is a lightweight, responsive desktop web browser built with **C++17** and
**Qt6 Widgets** on top of **QtWebEngine (Chromium)**.

This release (0.2.0) focuses on three things:

1. **A classic interface.** A reconstruction of Google Chrome 0.2/1.0 from 2009 — native window
   frame, menu bar, grey gradient tab strip, 23 px address bar with a page icon and a "go" arrow,
   bookmark bar and status bar. The modern dark chrome is still there; you choose at startup.
2. **CPU compatibility and cost control.** Automatic hardware tiering (Legacy / Low / Balanced /
   High) that decides Chromium's raster threads, renderer-process limit and whether the GPU process
   is used at all — plus a real Memory Saver that discards idle background renderers instead of
   printing a fake "Memory usage" number.
3. **Honest documentation.** What runs where, what does not, and why: see
   [`docs/PLATFORM_MATRIX.md`](docs/PLATFORM_MATRIX.md) and [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md).

> [!IMPORTANT]
> **Windows 7, 8 and 8.1 are not supported by this build, and cannot be.** Qt 6 dropped Windows
> 7/8.x targets and the Chromium inside QtWebEngine followed. The same applies to Android below API
> 26. One codebase cannot span that range; the three-branch plan is documented in
> [`docs/PLATFORM_MATRIX.md`](docs/PLATFORM_MATRIX.md), but only the desktop branch exists today.

> [!NOTE]
> Cloudar Browser was rewritten from a legacy Python/PyQt6 prototype into a native C++17
> application. The earlier Python codebase is preserved in [`legacy/`](legacy).

---

## ✨ Features

### 🕰️ Classic Interface (Chrome 2009)

- **Native window frame** with the platform's own title bar, as Chrome 0.2/1.0 looked on Windows 7.
- **Win32 menu bar**: File, Edit, View, History, Tools, Help — the same actions Chrome 2009 exposed
  through its page-control button and wrench menu.
- **Grey gradient tab strip** with square shoulders and the lighter active tab that joins the toolbar.
- **23 px omnibox** with a painted page icon inside the left edge and a green "go" arrow that appears
  while you type.
- **Status bar** with "Loading…", "Done" and a green progress sliver.
- **Settings, New Tab and About pages reskin themselves** to match, so the product stays coherent.

Details, and what was deliberately *not* reproduced: [`docs/CLASSIC_UI.md`](docs/CLASSIC_UI.md).

```bash
./build/cloudar-browser --classic     # or --modern, or Settings → Appearance
```

### ⚙️ CPU & Performance Compatibility

- **Startup hardware detection** (CPUID, RAM, core counts) chooses one of four tiers *before*
  `QApplication` exists, so the engine is configured correctly on the first tab.
- **Legacy tier**: software rasterizer, no GPU process, one raster thread, no UI animation — the
  only way a 2 GB machine runs a Chromium-based browser at all.
- **Real Memory Saver**: Light (2 h) / Medium (1 h) / High (15 min) / Ultra (2 min) idle thresholds
  actually tear down background renderers and wake them on click. Sessions survive restarts.
- **Honest reporting**: the hover cards show *Active / Background / Sleeping*, not an invented
  megabyte count. **Clean Memory Now** reports what it actually discarded.
- Overrides: `--perf-tier=<tier>`, `CLOUDAR_PERF_TIER`, or Settings → Performance.
- Full explanation: [`docs/CPU_COMPATIBILITY.md`](docs/CPU_COMPATIBILITY.md).

### 📑 Tabs

- **Vertical sidebar** (modern chrome): compact 54 px icon mode with hover cards, expanded mode with
  titles and close buttons, smooth width animation.
- **Horizontal tab strip** (both chromes; the only surface in classic mode) with active highlighting,
  audio indicators, drag-to-reorder and a `+` button that follows the last tab.
- **Ctrl+Click / middle-click** opens a new tab; `Ctrl+Shift+T` reopens the last closed one.
- **Session restore** including sleeping tabs.

### 🎬 Live Wallpaper (modern chrome)

- MP4/WebM video backgrounds on `cloudar://newtab`, cached in IndexedDB, with play/pause, mute,
  speed and volume controls. Disabled in classic mode, where CPU is the scarce resource.

### 🛡️ AdBlocker, AI panel, split view

- Request interception against ad/tracker domains, with a blocked counter in the status bar.
- Dockable AI assistant panel (`Ctrl+Shift+A`) with quick switching between providers.
- Side-by-side split view for two independent pages.
- All three remain available in the classic chrome through the **View** menu.

### 🔒 Privacy & permissions

- **Incognito**: off-the-record profile, in-memory cache, no persistent cookies.
- **Per-origin permission prompts** for camera, microphone, geolocation, notifications, clipboard.
- Incognito is **not** anonymity — read [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) §5 before
  relying on it.

### 🌐 Internal pages (`cloudar://`)

`newtab` · `settings` · `bookmarks` · `history` · `downloads` · `extensions` · `permissions` ·
`flags` · `about`

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|:---|:---|
| `Ctrl + T` | New tab |
| `Ctrl + W` | Close tab |
| `Ctrl + Shift + T` | Reopen closed tab |
| `Ctrl + L` / `Alt + D` | Focus address bar |
| `Ctrl + R` / `F5` | Reload |
| `Ctrl + Shift + R` | Hard reload (bypass cache) |
| `Ctrl + Tab` / `Ctrl + Shift + Tab` | Next / previous tab |
| `Ctrl + 1` … `Ctrl + 9` | Jump to tab |
| `Ctrl + Shift + S` | Toggle vertical sidebar / side panel |
| `Ctrl + Shift + A` | AI assistant panel |
| `Ctrl + Shift + B` | Show / hide bookmarks bar |
| `Ctrl + D` | Bookmark current page |
| `Ctrl + B` / `Ctrl + H` / `Ctrl + J` | Bookmarks / History / Downloads |
| `Ctrl + F` | Find in page |
| `Ctrl + +` / `Ctrl + -` / `Ctrl + 0` | Zoom in / out / reset |
| `F11` | Fullscreen |

---

## 🚀 Building

### Requirements

| | Minimum | Recommended |
|:---|:---|:---|
| Compiler | C++17 (GCC 10, Clang 12, MSVC 2019) | GCC 13, Clang 17, MSVC 2022 |
| Qt | 6.5 LTS (`Core Gui Widgets WebEngineWidgets WebEngineCore Network WebChannel`) | 6.8 LTS or 6.10 |
| CMake | 3.16 | 3.28 |
| CPU | x86-64 with SSE3 (see note) | anything from 2013 onwards |

> SSE3 is Chromium's floor, and the official Qt builds assume SSE4.2-era hardware. There is no
> supported switch to relax it — see [`docs/CPU_COMPATIBILITY.md`](docs/CPU_COMPATIBILITY.md) §4.

### Linux

```bash
# Debian 12/13, Ubuntu 22.04/24.04+
sudo apt install -y build-essential cmake ninja-build zlib1g-dev \
                    qt6-base-dev qt6-webengine-dev qt6-webengine-dev-tools \
                    libqt6webenginewidgets6 libqt6webchannel6-dev libgl1 libegl1

cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/cloudar-browser
```

Debian 11 and Ubuntu 20.04 have **no** `qt6-webengine-dev`; Qt 6.5+ is required, so those
distributions need a self-built Qt or the legacy branch. See
[`docs/PLATFORM_MATRIX.md`](docs/PLATFORM_MATRIX.md).

### Windows 10 / 11

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build
windeployqt --release --webenginewidgets build/cloudar-browser.exe
```

Windows 7/8/8.1 will not build or run: Qt 6 does not target them.

### Command-line options

```
--classic / --modern        force an interface for this launch (does not change the saved choice)
--perf-tier <tier>          auto | legacy | low | balanced | high (this launch only)
-i, --incognito             private window
--version / --help
```

---

## 📂 Project Structure

```text
Cloudar-Browser/
├── src/
│   ├── main.cpp                 # startup: perf tier, theme, CLI
│   ├── MainWindow.*             # window, both chromes, menus, omnibox decor
│   ├── ClassicTheme.*           # ThemeStyle, per-chrome metrics, QSS, painted icons
│   ├── PerformanceProfile.*     # CPUID/RAM detection, Chromium switches, animation policy
│   ├── MemorySaver.*            # real idle-tab discard and statistics
│   ├── BrowserTabWidget.*       # tab host, sleep/wake, thumbnails, closed-tab stack
│   ├── BrowserSidebar.*         # vertical tabs + hover cards
│   ├── TabPreviewPopup.*        # hover thumbnail card
│   ├── SettingsBridge.*         # cloudar://settings backend
│   ├── Language.*               # dictionaries, QTranslator, languageChanged
│   ├── AdBlock.*, AISidebar.*, SplitView.*, ReaderMode.*, ThemeManager.*, ...
├── resources/                   # internal pages, icons, QSS assets
├── docs/
│   ├── CLASSIC_UI.md            # the Chrome 2009 interface, in detail
│   ├── CPU_COMPATIBILITY.md     # hardware tiers and what they change
│   ├── LANGUAGE.md              # interface languages: how a string is translated
│   ├── PLATFORM_MATRIX.md       # what runs where; legacy/mobile branch plan
│   └── KNOWN_ISSUES.md          # honest list of gaps and limitations
└── legacy/                      # archived Python / PyQt6 prototype
```

---

## 📊 Status

| Area | State |
|:---|:---|
| Linux (Debian 12/13, Ubuntu 22.04+) | built and smoke-tested in CI |
| Windows 10/11 | build job in CI; installer and code signing not yet done |
| Qt 6.5 LTS / 6.8 LTS / 6.10 | 6.8 LTS and 6.10 are built in CI; 6.5 is supported in the source (`QT_VERSION` guards) but not exercised by CI |
| Memory Saver, CPU tiers | implemented; **no unit tests yet** — see `docs/KNOWN_ISSUES.md` §9 |
| Extensions | partial `chrome.*` shim — see `docs/KNOWN_ISSUES.md` §4 |
| Interface languages | 10 languages, native chrome + internal pages; settings body copy still English — see `docs/LANGUAGE.md` |
| Windows 7/8.1, Android, Debian 11 | **not** supported by this branch, on purpose |

---

## 📄 License

MIT — see [LICENSE](LICENSE).
