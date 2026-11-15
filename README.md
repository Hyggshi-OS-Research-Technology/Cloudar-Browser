# 🌐 Cloudar Browser™

<p align="center">
  <img src="resources/icon.png" width="128" height="128" alt="Cloudar Browser Icon" />
</p>

<p align="center">
  <strong>A modern, high-performance web browser engineered in C++17 and Qt6 (QtWebEngine / Chromium).</strong>
</p>

<p align="center">
  <a href="https://github.com/Hyggshi-OS-Research-Technology/Cloudar-Browser/actions"><img src="https://img.shields.io/github/actions/workflow/status/Hyggshi-OS-Research-Technology/Cloudar-Browser/build.yml?branch=main&label=CI%20Build&logo=github" alt="CI Status" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg?logo=c%2B%2B" alt="C++17" />
  <img src="https://img.shields.io/badge/Qt-6.4%20--%206.10+-41CD52.svg?logo=qt" alt="Qt6" />
  <img src="https://img.shields.io/badge/Engine-QtWebEngine%20(Chromium)-orange.svg" alt="QtWebEngine" />
  <img src="https://img.shields.io/badge/Platform-Linux%20x86__64-yellow.svg?logo=linux" alt="Platform" />
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License" />
</p>

---

## 📖 Overview

**Cloudar Browser™** is a lightweight, responsive, and privacy-focused desktop web browser built with pure **C++17** and **Qt6 Widgets** on top of the **QtWebEngine (Chromium)** engine.

Designed for speed, multitasking, and personalization, Cloudar Browser pairs native desktop performance with modern conveniences:
- **Compact & Collapsible Vertical Tabs** with real-time hover cards and thumbnail previews
- **Animated Video & Live Wallpaper Backgrounds** on the New Tab page
- **Built-in AdBlock** request filtering
- **Dockable AI Assistant Sidebar** (`Ctrl+Shift+A`)
- **Dual-Pane Split View** (`⧉`) for side-by-side browsing
- **Chrome / WebExtension support** (Manifest V3)
- Dedicated internal pages served securely via `cloudar://`

> [!NOTE]
> Cloudar Browser was completely rewritten from its legacy Python/PyQt6 prototype into a native C++17 application for dramatically reduced startup latency, lower RAM usage, and instant UI responsiveness. The previous Python codebase is preserved in the [`legacy/`](file:///home/hyggshi/Downloads/Cloudar-Browser/legacy) directory.

---

## ✨ Features

### 📑 Intelligent Tab Management
- **Collapsible Vertical Sidebar**:
  - **Compact Mode (54px)**: Minimalist icon-only sidebar maximizing viewing area.
  - **Expanded Mode (240px+)**: Displays full page titles and close buttons.
  - Smooth animation toggle via the hamburger icon (`☰` / `‹`) or splitter drag.
  - **Floating Hover Cards**: Hovering over any tab in compact mode pops up a real-time thumbnail preview, page URL, and memory metrics.
- **Horizontal Tab Strip**: Alternative classic tab bar with active tab highlighting, audio indicators, close buttons, and `+` new tab button.
- **Session Restoration**: Seamlessly reloads your previous tabs on next launch.

### 🎬 Live Wallpaper & Video Backgrounds
- Supports MP4 and WebM video files as live animated backgrounds on `cloudar://newtab`.
- High-capacity local caching via **IndexedDB** prevents storage limitations.
- Built-in media controls: Play / Pause, Mute / Unmute, Playback speed selector (0.5x to 2x), and volume adjustment.
- Instant fallback to static custom images or vibrant accent colors.
- Chromium autoplay flags (`--autoplay-policy=no-user-gesture-required`) configured for smooth background video start.

### 🛡️ Network AdBlocker
- Integrated request interceptor that filters ad networks, malicious domains, and analytics trackers.
- Real-time blocked request counter displayed cleanly in the status bar.
- Low overhead with zero impact on page load speed.

### 🤖 AI Assistant Sidebar (`Ctrl+Shift+A`)
- Dockable right-hand panel for your favorite AI tools without leaving the current tab.
- Quick switcher for **Google Gemini**, **ChatGPT**, **Claude**, **DeepSeek**, **Perplexity**, **Microsoft Copilot**, or custom URLs.

### ↔️ Dual-Pane Split View
- Split your browser viewport into two independent web pages side-by-side (`⧉` button in toolbar).
- Perfect for research, coding, documentation review, or simultaneous video viewing.

### 📥 Media & Download Helpers
- **YouTube Downloader Bridge**: Integrates with `yt-dlp` to download video/audio in one click.
- **Torrent Bridge**: Magnet link detection with fast hand-off.
- **Download Manager (`Ctrl+J`)**: Detailed progress, download speeds, and file management.

### 🧩 Chrome & WebExtension Support
- Supports unpacked Chrome extensions (Manifest V3) and `.crx` file installation.
- Extension management dialog (`cloudar://extensions`).

### 🔒 Privacy, Security & Permissions
- **Incognito Mode**: Private window with isolated in-memory profile (no history, cookies, or cache retained on disk).
- **Per-Origin Permission Manager**: Native Qt6 permission prompts for Camera, Microphone, Geolocation, and Desktop Notifications (`cloudar://permissions`).

### 🎨 Frameless Modern UI & Custom Themes
- Custom draggable frameless titlebar with embedded system controls.
- Dynamic accent color theming (Blue, Purple, Emerald, Orange, Crimson, Midnight).
- Dark-mode optimized styling with translucent overlays and subtle drop shadows.

### 🌐 Secure Internal Pages (`cloudar://`)
All internal browser functions are hosted under the `cloudar://` scheme:
- `cloudar://newtab` — Customizable dashboard with live wallpaper support
- `cloudar://settings` — Appearance, search engines, privacy, and browser configuration
- `cloudar://bookmarks` — Visual bookmark manager (`Ctrl+B`)
- `cloudar://history` — Fast browsing history search (`Ctrl+H`)
- `cloudar://downloads` — Active and completed downloads (`Ctrl+J`)
- `cloudar://extensions` — Extensions viewer and `.crx` loader
- `cloudar://about` — Version information and environment specifications
- `cloudar://flags` — Chromium runtime and experimental flags

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|:---|:---|
| `Ctrl + T` | Open a new tab |
| `Ctrl + W` | Close the current tab |
| `Ctrl + Shift + T` | Reopen last closed tab |
| `Ctrl + L` / `Alt + D` | Focus address bar |
| `Ctrl + R` / `F5` | Reload current page |
| `Ctrl + Shift + R` | Hard reload (bypass cache) |
| `Ctrl + Tab` | Switch to next tab |
| `Ctrl + Shift + Tab` | Switch to previous tab |
| `Ctrl + 1` .. `Ctrl + 9` | Jump directly to tab index 1–9 |
| `Ctrl + Shift + S` | Toggle Vertical Tab Sidebar |
| `Ctrl + Shift + A` | Toggle AI Assistant Sidebar |
| `Ctrl + D` | Bookmark current page |
| `Ctrl + B` | Open Bookmarks page |
| `Ctrl + H` | Open History page |
| `Ctrl + J` | Open Downloads manager |
| `Ctrl + F` | In-page Find bar |
| `Ctrl + +` / `Ctrl + -` | Zoom in / Zoom out |
| `Ctrl + 0` | Reset zoom to 100% |
| `F11` | Fullscreen mode |

---

## 🚀 Building & Running

### 1. Prerequisites

#### Ubuntu / Debian (22.04, 24.04 LTS):
```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build zlib1g-dev \
                    qt6-base-dev qt6-webengine-dev qt6-webengine-dev-tools \
                    libqt6webenginewidgets6 libqt6webchannel6-dev \
                    libgl1 libegl1
```

#### Fedora:
```bash
sudo dnf install -y gcc-c++ cmake ninja-build zlib-devel \
                    qt6-qtbase-devel qt6-qtwebengine-devel qt6-qtwebchannel-devel
```

#### Arch Linux:
```bash
sudo pacman -S --needed base-devel cmake ninja zlib qt6-base qt6-webengine qt6-webchannel
```

---

### 2. Compile with CMake & Ninja

```bash
# Configure the build directory
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile using all available CPU cores
ninja -C build -j$(nproc)
```

---

### 3. Launch

You can run the browser using the convenience script:
```bash
./run_cpp.sh
```

Or execute the compiled binary directly:
```bash
./build/cloudar-browser
```

To launch directly in Incognito mode:
```bash
./build/cloudar-browser --incognito
```

---

## 📂 Project Structure

```text
Cloudar-Browser/
├── CMakeLists.txt              # Primary CMake build configuration
├── run_cpp.sh                  # Launch script with proper library paths
├── resources/                  # UI assets and internal pages
│   ├── icon.png                # Official Cloudar Browser icon
│   ├── resources.qrc           # Qt Resource Collection
│   ├── newtab.html             # New tab page with live video wallpaper support
│   ├── settings.html           # Settings UI
│   ├── about.html              # About page
│   ├── version.html            # Version info page
│   ├── flags.html              # Browser flags page
│   └── cloudar-browser.desktop # Linux desktop integration
├── src/                        # Native C++17 source code
│   ├── main.cpp                # Application entrypoint & Chromium flag config
│   ├── MainWindow.h/.cpp       # Main browser window, layout, and toolbars
│   ├── BrowserTabWidget.h/.cpp # Multi-tab host and WebEngine view manager
│   ├── BrowserSidebar.h/.cpp   # Collapsible vertical tab sidebar & hover cards
│   ├── AdBlock.h/.cpp          # Network request filtering & tracker blocker
│   ├── AISidebar.h/.cpp        # Dockable AI assistant pane
│   ├── SplitView.h/.cpp        # Side-by-side dual viewport
│   ├── LightSchemeHandler.h    # Custom cloudar:// scheme URL protocol handler
│   ├── DownloadManager.h/.cpp  # File download supervisor
│   ├── BookmarkManager.h/.cpp  # Bookmark database and bookmark bar
│   ├── HistoryManager.h/.cpp   # Browsing history manager
│   ├── ThemeManager.h/.cpp     # Dynamic stylesheet and accent color engine
│   ├── TitleBar.h/.cpp         # Custom frameless window titlebar
│   ├── YouTubeDownloader.h     # yt-dlp media downloader integration
│   ├── TorrentDownloader.h     # Torrent magnet handler
│   └── WebExtensionManager.h   # Manifest V3 extension support
├── packaging/                  # Linux distribution packaging scripts (.deb)
└── legacy/                     # Archived Python / PyQt6 prototype
```

---

## 📄 License

Cloudar Browser is open-source software licensed under the **[MIT License](LICENSE)**.
