# Cloudar Browser — Documentation

Cloudar Browser is a C++17 / Qt 6 desktop browser built on QtWebEngine, with two
complete user interfaces (a Chrome 2009 reconstruction and a modern dark chrome).
Start with the top-level [README](../README.md) for installation; the documents
here cover the parts that need more than a paragraph.

## Read these first

| Document | What it answers |
|:---|:---|
| [PLATFORM_MATRIX.md](PLATFORM_MATRIX.md) | Which Windows/Linux/Android versions actually run, and why the rest need separate legacy/mobile branches. |
| [CPU_COMPATIBILITY.md](CPU_COMPATIBILITY.md) | How the Legacy/Low/Balanced/High tiers are detected, what Chromium switches each one sets, and the hard SSE3 floor. |
| [CLASSIC_UI.md](CLASSIC_UI.md) | What the Chrome 2009 interface reproduces exactly, what it deliberately does not, and how to add a third chrome. |
| [KNOWN_ISSUES.md](KNOWN_ISSUES.md) | Everything the browser does not do yet: extension gaps, incognito caveats, missing benchmarks and tests. |

## Feature notes

| Document | Subject |
|:---|:---|
| [RAM_MONITORING.md](RAM_MONITORING.md) | Memory reporting. Explains why the old per-tab "Memory usage: N MB" numbers were removed and what a correct implementation would look like. |
| [FILE_PROTOCOL_SECURITY.md](FILE_PROTOCOL_SECURITY.md) | `file://` handling; what local content may and may not reach. |
| [IMAGE_OPTIMIZATION.md](IMAGE_OPTIMIZATION.md) | New Tab background image storage and resizing. |
| [QUICKSTART.md](QUICKSTART.md) | Build, run and troubleshoot in five minutes. |

## Agent / contributor notes

| Document | Subject |
|:---|:---|
| [AGENTS.md](AGENTS.md) | Repository guidance for automated coding agents. Describes the **legacy Python prototype** in `legacy/` — the native tree under `src/` is the current implementation and does not share its module layout. |
| [CLAUDE.md](CLAUDE.md) | The same guidance in Claude-specific form. |

## Where the code lives

```text
src/ClassicTheme.*        both chrome stylesheets, per-chrome metrics, painted icons
src/PerformanceProfile.*  CPUID/RAM detection, Chromium switches, animation policy
src/MemorySaver.*         idle-tab discard, sleep placeholders, statistics
src/MainWindow.*          window, menus, omnibox decoration, chrome switching
src/BrowserTabWidget.*    tab host, sleep/wake, thumbnails, closed-tab stack
src/SettingsBridge.*      backend for cloudar://settings
resources/*.html          internal pages; each renders a classic and a modern variant
```
