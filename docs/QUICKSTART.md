# Quick Start

Cloudar Browser 0.2.0 — Qt 6 / QtWebEngine, C++17.

## 1. Install the toolchain

**Debian 12/13, Ubuntu 22.04/24.04+**

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build zlib1g-dev \
                    qt6-base-dev qt6-webengine-dev qt6-webengine-dev-tools \
                    libqt6webenginewidgets6 libqt6webchannel6-dev \
                    libgl1 libegl1
```

**Fedora**

```bash
sudo dnf install -y gcc-c++ cmake ninja-build zlib-devel \
                    qt6-qtbase-devel qt6-qtwebengine-devel qt6-qtwebchannel-devel
```

**Arch**

```bash
sudo pacman -S --needed base-devel cmake ninja zlib qt6-base qt6-webengine qt6-webchannel
```

**Windows 10/11** — install Qt 6.8 LTS (or 6.10) with the *QtWebEngine* and
*Qt WebChannel* modules from the Qt online installer, plus CMake and Ninja.

> Debian 11 and Ubuntu 20.04 do not ship `qt6-webengine-dev`. Qt 6.5+ is
> required, so those systems need a self-built Qt or the legacy branch — see
> [`PLATFORM_MATRIX.md`](PLATFORM_MATRIX.md). Windows 7/8/8.1 cannot run Qt 6
> at all.

## 2. Build

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

Windows:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build
windeployqt --release --webenginewidgets build/cloudar-browser.exe
```

## 3. Run

```bash
./build/cloudar-browser              # classic Chrome 2009 interface (default)
./build/cloudar-browser --modern     # dark modern interface
./build/cloudar-browser --classic    # force the classic interface
./build/cloudar-browser -i           # incognito window
./build/cloudar-browser https://example.com
```

The interface choice, accent colour and hardware settings also live in
`cloudar://settings`:

| Page | What is there |
|:---|:---|
| General | home page, search engine, session restore |
| Appearance | interface style (classic/modern), accent colour, Home button, bookmarks bar |
| Performance & Memory | memory saver level, CPU tier, hardware acceleration, animations, live sleeping-tab stats |

## 4. If it starts slowly, or the UI is choppy

Check **Settings → Performance & Memory → CPU & hardware compatibility**. The
detected tier and the reason for it are shown there. To force a tier:

```bash
CLOUDAR_PERF_TIER=legacy ./build/cloudar-browser
./build/cloudar-browser --perf-tier=low
```

Details: [`CPU_COMPATIBILITY.md`](CPU_COMPATIBILITY.md).

## 5. Keyboard shortcuts

| Shortcut | Action |
|:---|:---|
| `Ctrl+T` / `Ctrl+W` | New tab / close tab |
| `Ctrl+Shift+T` | Reopen closed tab |
| `Ctrl+L` | Focus the address bar |
| `Ctrl+R` / `F5` | Reload |
| `Ctrl+Shift+B` | Show/hide bookmarks bar |
| `Ctrl+Shift+S` | Side panel |
| `Ctrl+Shift+A` | AI assistant panel |
| `Ctrl+D` | Bookmark this page |
| `Ctrl+B` / `Ctrl+H` / `Ctrl+J` | Bookmarks / History / Downloads |
| `Ctrl+F` | Find in page |

## 6. Troubleshooting

| Symptom | Fix |
|:---|:---|
| `Qt6WebEngineWidgets` not found | Install `qt6-webengine-dev` (Debian/Ubuntu) or the QtWebEngine module (Windows installer). |
| Blank page / immediate crash on old hardware | Force the legacy tier: `CLOUDAR_PERF_TIER=legacy`. |
| Flickering page content | Disable hardware acceleration in Settings → Performance (restart required). |
| No window at all under a remote/headless X server | QtWebEngine needs a real GL context; use `QT_OPENGL=software`. |
| Interface style change "did nothing" | The window frame and menu layout only change after a restart; use the restart button on the Appearance page. |
