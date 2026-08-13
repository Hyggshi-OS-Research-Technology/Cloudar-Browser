# 🌐 Cloudar Browser™

A modern, feature-rich web browser built with **Python + PyQt6** and **QtWebEngine**.
It ships a Chrome-inspired UI with tabbed browsing, incognito mode, built-in
AdBlock, an AI assistant sidebar, extensions (YouTube / media / torrent
downloaders), split-screen viewing, vertical tabs and much more.

![Python](https://img.shields.io/badge/Python-3.8+-blue.svg)
![PyQt6](https://img.shields.io/badge/PyQt6-6.6+-green.svg)
![QtWebEngine](https://img.shields.io/badge/QtWebEngine-6.6+-orange.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)

---

## ✨ Features

- **🗂️ Tabbed browsing** — horizontal *and* vertical tab strips, tab
  duplication, hover previews, and session persistence.
- **🙈 Incognito mode** — separate profile; no history, cookies, cache or form
  data are written after the window closes (`Ctrl+Shift+N`).
- **🛡️ Built-in AdBlock** — a network-level request interceptor
  (`features/adblock.py`) blocks ad/tracker domains on **all** sites, plus a
  cosmetic content-script layer that hides ad elements and auto-skips / mutes
  YouTube ads. A status-bar counter shows how many requests were blocked, with
  an experimental v2 engine behind a flag.
- **🤖 AI Assistant sidebar** — chat, page summarization, translation, link
  extraction, and "explain code" for the current selection (`Ctrl+Shift+A`).
- **⬇️ Download manager** — live progress, a Chrome-style download bubble, and
  full support for `ask-before-download` + a configurable download folder.
- **🎬 YouTube & media downloader** (extension) — detects videos and images on
  pages (including YouTube), lets you pick quality/resolution, choose original
  high-res images via `srcset`, multi-select, and filter by format. Powered by
  `yt-dlp`.
- **🧲 Torrent downloader** (extension) — detects `.torrent` files and `magnet:`
  links and downloads them with real-time progress (via libtorrent).
- **↔️ Split screen** — browse two pages side by side in one tab.
- **🌍 Page translation** — one-click Google Translate for the current page.
- **🔍 Find in page**, **🔍 smart address bar** (URL autocomplete + search).
- **⭐ Bookmarks**, **📜 history**, with dedicated internal pages.
- **🔒 Permissions manager** — per-site permission prompts (camera, mic,
  location, notifications & more) with remembered decisions.
- **🧪 Experiments / flags** — a `cloudar://flags` page to toggle experimental
  features (see below).
- **⚙️ Customizable settings** — home page, search engine, theme, download
  location, privacy, efficiency mode, and more.
- **🌐 Multi-language UI** — English, Vietnamese, Chinese, French, German,
  Spanish, Korean, Russian, Portuguese, Japanese.
- **📱 Android / mobile build** — automatically falls back to a QtWebView-based
  mobile browser when running on Android.
- **🎨 Customizable New Tab page** with user-set background images.

---

## 📋 Requirements

- **Python 3.8+**
- **PyQt6** (>= 6.6)
- **PyQt6-WebEngine** (>= 6.6)
- **Pillow** (for resource/icon handling)
- Optional: **yt-dlp** for the YouTube downloader, **libtorrent** for torrents.

> [!IMPORTANT]
> Qt/PyQt6 versions should be kept in sync. The Android build uses
> `PyQt6-Qt6` (QtWebEngine is **not** supported on mobile) — see `pyproject.toml`.

---

## 🚀 Installation

### 1. Clone / download the repository

```bash
git clone <repo-url>
cd "Hyggshi browser Python"
```

### 2. Install dependencies

```bash
pip install -r requirements.txt
```

Or install manually:

```bash
pip install PyQt6 PyQt6-WebEngine Pillow
```

### 3. Run the browser

```bash
# Directly
python main.py

# Or using the provided launcher (adds a libpthread LD_PRELOAD fix on Linux)
./run.sh
```

> [!IMPORTANT]
> **Windows users:** if you hit a `DLL load failed` error, install the
> **Visual C++ Redistributable** from
> <https://aka.ms/vs/17/release/vc_redist.x64.exe>, restart, and run again.

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+T` | New tab |
| `Ctrl+W` | Close current tab |
| `Ctrl+N` | New window |
| `Ctrl+Shift+N` | New incognito window |
| `Ctrl+D` | Bookmark current page |
| `Ctrl+H` | Show history |
| `Ctrl+J` | Show downloads |
| `Ctrl+L` | Focus the address bar |
| `Ctrl+F` | Find in page |
| `Ctrl+S` | Save page as |
| `Ctrl+0` | Reset zoom |
| `Ctrl+Q` | Quit browser |
| `F5` / zoom shortcuts | Reload / zoom in / zoom out |
| `F12` | Developer tools |
| `Ctrl+Shift+A` | Toggle AI sidebar |


---

## 🧪 Experiments (Flags)

Open `cloudar://flags` to toggle experimental features. Some require a relaunch:

| Flag | Category | Description |
|------|----------|-------------|
| `download-bubble` | UI | Chrome-style download status popup |
| `vertical-tabs-default` | UI | Start with vertical tabs enabled |
| `tab-hover-preview` | UI | Page thumbnail preview on tab hover |
| `smooth-scrolling` | Performance | Animated page scrolling |
| `gpu-rasterization` | Performance | GPU page rendering |
| `parallel-downloads` | Downloads | Split downloads into multiple connections |
| `adblock-v2-engine` | Privacy | Rewritten, faster ad-block engine |
| `ai-sidebar-streaming` | AI | Stream AI replies word-by-word |

---

## 🔌 Custom URL Schemes & Internal Pages

The browser registers custom schemes **before** the Qt app starts (see `main.py`):

- **`cloudar://`** — internal pages: `newtab`, `bookmarks`, `history`,
  `downloads`, `settings`, `extensions`, `permissions`, `flags`, `about`.
- **`cloudar-asset://`** — serves user assets such as background images.

---

## 🧩 Extensions

Extensions live under `extensions/` (canonical copies are also mirrored under
`features/extensions/`). They are injected by `ExtensionManager` using content
scripts (`background.js` + optional `manifest.json`):

| Extension | Purpose |
|-----------|---------|
| **Video Downloader** | Detects videos/images on any page (incl. YouTube). Uses `yt-dlp`; supports quality picker, original-image selection, format filters and multi-select. |
| **AdBlock** | Cosmetic layer that hides ad elements and auto-skips/mutes video ads. |
| **Torrent Downloader** | Detects `.torrent` / `magnet:` links and downloads via bridge `torrentDownloader`. |

> Each extension requiring Python interop exposes a dedicated **QWebChannel**
> bridge (e.g. `youtubeDownloader`, `torrentDownloader`) that is attached only
> to the pages that need it — never to arbitrary third-party sites.

