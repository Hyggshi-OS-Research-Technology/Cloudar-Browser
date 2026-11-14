# 🌐 Cloudar Browser™

A modern, high-performance web browser built with **C++17**, **Qt6 Widgets**, and **QtWebEngine** (Chromium).
It features a clean Chrome-inspired UI with tabbed browsing, incognito mode, built-in AdBlock, AI assistant sidebar, video & live wallpaper backgrounds, extension support, split-screen viewing, vertical tabs with preview popups, and much more.

![C++17](https://img.shields.io/badge/C++-17-blue.svg)
![Qt6](https://img.shields.io/badge/Qt6-6.6+-green.svg)
![QtWebEngine](https://img.shields.io/badge/QtWebEngine-6.6+-orange.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)

---

## 🚀 Building & Running (C++)

### Prerequisites (Ubuntu/Debian)
```bash
sudo apt install build-essential cmake ninja-build zlib1g-dev \
                 qt6-base-dev qt6-webengine-dev qt6-webengine-dev-tools \
                 libqt6webenginewidgets6 libqt6webchannel6-dev
```

### Build with CMake & Ninja
```bash
cmake -B build -S . -G Ninja
ninja -C build
```

### Launch
```bash
./run_cpp.sh
# or directly:
./build/cloudar-browser
```

---

## ✨ Features

- **⚡ Blazing Fast C++ Engine** — native compiled C++17 binary with lightweight memory footprint.
- **🎥 Video & Live Wallpaper Backgrounds** — set animated MP4/WebM videos (e.g. Live2D wallpapers) as your New Tab background with sound and play/pause controls, backed by high-capacity IndexedDB.
- **🗂️ Advanced Tab Management** — horizontal & vertical tab strips, tab hover previews, audio indicator, middle-click close, and session restore.
- **🛡️ Built-in AdBlock** — network-level request interceptor blocking ad & tracking domains with status-bar block counter.
- **🤖 AI Assistant Sidebar** — dockable AI pane supporting ChatGPT, Claude, Gemini, DeepSeek, Copilot, Perplexity, and more (`Ctrl+Shift+A`).
- **↔️ Split Screen Dual View** — browse two websites simultaneously side by side (`⧉` button).
- **🧩 WebExtensions & Chrome Extensions** — supports unpacked Manifest V3 extensions, content scripts, and `.crx` installer.
- **📖 Reader Mode** — distraction-free readability view with clean typography.
- **🔒 Security & Permissions** — per-origin permission prompts (camera, mic, location, notifications) using Qt6 `QWebEnginePermission`.
- **⭐ Bookmarks & History** — bookmark bar, bookmark manager, and flat visit history.
- **🎨 Theming & Frameless Titlebar** — draggable custom titlebar with synchronized accent color theme.
- **🌐 Internal Pages** — custom `cloudar://` pages (`newtab`, `settings`, `about`, `bookmarks`, `history`, `downloads`, `extensions`, `flags`, `permissions`).

---

## 📦 Legacy Python Version

The previous Python/PyQt6 version of Cloudar Browser has been archived in the [`legacy/`](file:///home/hyggshi/Downloads/Cloudar-Browser/legacy) directory for reference.
