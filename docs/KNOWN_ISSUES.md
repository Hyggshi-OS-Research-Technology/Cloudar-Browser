# Known Issues & Honest Limitations

This file exists because the rest of the documentation describes what Cloudar
*does*, and this file describes what it does **not** do yet. Everything below
was verified against the code in this repository rather than against older
README claims.

Last reviewed: 2026-11-24, Cloudar Browser 0.2.0.

---

## 1. Platform support is narrower than "Windows 7 to 11, Debian 11 to 14, Android 3.4+"

The matrix in [`PLATFORM_MATRIX.md`](PLATFORM_MATRIX.md) is the authoritative
statement of what is actually buildable and testable. The short version:

| Requested | Reality with the current single codebase |
|:---|:---|
| Windows 10 / 11 | Supported. Qt 6.5 LTS / 6.8 LTS / 6.10+ from the Qt online installer, MSVC 2022. |
| Windows 8 / 8.1 | **Not supported by Qt 6 at all.** Qt 6 dropped Windows 7 and 8.x targets; so did Chromium. Requires the `legacy` branch described in `PLATFORM_MATRIX.md`. |
| Windows 7 | Same as above. There is no supported path with QtWebEngine 6.x. |
| Debian 11 (bullseye) | **No Qt 6 WebEngine packages** in the distribution. Either build Qt from source (Qt 6.2 is the last line that builds with GCC 10) or use the `legacy` branch. |
| Debian 12 / 13, Ubuntu 22.04+ | Supported from distro packages. |
| Ubuntu 20.04 | No `qt6-webengine-dev`; needs the Qt online installer or the `legacy` branch. |
| Android 8+ (API 26+) | Possible with Qt 6.5 LTS (min API 26), but there is **no mobile frontend in this repository yet**. |
| Android 3.4 – 7.x (API 10–24) | **Not possible with Qt 6.** Qt 6 needs API 26+, and QtWebEngine has never shipped a mobile target. Needs a separate Android frontend built on `android.webkit.WebView`. |

Building the same source with a different toolchain does not change this:
Qt 6 and the Chromium inside QtWebEngine compile against Windows APIs that do
not exist on Windows 7, and against Android APIs that do not exist before API
26.

## 2. The browser engine's security level is tied to the Qt version

QtWebEngine inherits whatever Chromium the Qt release was branched from. Qt
does backport security fixes to LTS Chromium branches, but an LTS branch still
ages. `cloudar://about` reports the exact Chromium version at runtime; compare
it to the current stable Chromium before treating this browser as safe for
hostile content. See `PLATFORM_MATRIX.md` §"Chromium versions".

## 3. "Legacy OS support" and "current engine" are mutually exclusive

You can have an interface and a feature set that runs on Windows 7, or an
engine that receives upstream security fixes. In 2026 you cannot have both in
one binary. That is the entire reason for the proposed branch split:

- `cloudar-desktop` (this branch): Qt 6 + current-ish Chromium, Windows 10/11,
  modern Linux.
- `cloudar-legacy`: Qt 5.15 LTS with QtWebEngine 5.15 (Chromium 87, last
  patched by Qt in 2023). Windows 7/8.1, Debian 10/11, Ubuntu 18.04/20.04.
  Frozen, clearly labelled as unmaintained for security purposes.
- `cloudar-mobile`: Android frontend on `android.webkit.WebView` (which is
  updated through Google Play, so it stays current on Android 5+), sharing
  the internal pages and the settings model, not the Qt UI.

## 4. Extension support is partial

`chrome-api-shim.js` + `WebExtensionManager` implement a subset of
`chrome.runtime`, `chrome.tabs`, `chrome.storage`, `chrome.scripting` and
`chrome.action`. Known gaps:

- No `chrome.webRequest` blocking on arbitrary URLs (only the built-in AdBlock
  interceptor sees requests).
- No `chrome.declarativeNetRequest`.
- No service-worker extension background contexts: MV3 background scripts are
  emulated in a page context.
- `chrome.alarms`, `chrome.i18n`, `chrome.contextMenus` and
  `chrome.notifications` are no-ops.
- `.crx` install checks the ZIP structure but **not** the CRX signature.
  Installing a CRX from an untrusted source is equivalent to running its code.

There is no automated extension test suite. Treat "works with extension X" as
unverified until someone tests it on this build.

## 5. Incognito is not anonymity

Incognito mode here uses an off-the-record `QWebEngineProfile` with an
in-memory cache and no persistent cookies. It does **not**:

- change your IP address (that is Tor/proxy territory, see the Tor panel),
- prevent WebRTC from leaking local addresses unless the "Block WebRTC"
  setting is on,
- resist browser fingerprinting (canvas, fonts, timezone and screen metrics are
  all readable),
- stop DNS queries from being visible to your DNS resolver,
- hide traffic volume or timing from your network.

## 6. Memory Saver is real now, but per-tab RAM is still an estimate

`MemorySaver` genuinely tears down background renderers and the settings page
reports how many tabs are asleep. The *number of megabytes* it reports is an
estimate: QtWebEngine exposes no per-tab RSS, and getting it would require
walking `/proc/<pid>/smaps` on Linux and `GetProcessMemoryInfo` on Windows,
mapping Chromium's process tree back to tabs. Until that exists, the UI labels
the figure as an estimate and never presents it as a measurement.

Live wallpaper and split view also cost real RAM and GPU time; there is no
automated benchmark in this repository yet.

## 7. UI features that are declared but not implemented

- The split view and AI sidebar have no keyboard-less discoverability in the
  classic chrome (they are in **View**).
- `cloudar://flags` is a UI over `QSettings`; the individual switches are not
  wired to Chromium command-line features, and labels say which ones are
  cosmetic.
- Reader mode is a heuristic DOM rewrite, not Readability.js.

## 8. Performance claims still need measurements

The theoretical costs are known: a live wallpaper is a decoding video element,
split view is a second engine view, and each live tab costs a Chromium renderer.
Memory Saver removes the renderer cost of *idle* tabs, and the CPU tiers reduce
raster threads and disable GPU compositing where it hurts most.

What does not exist yet is a **benchmark**: there are no numbers in this
repository for peak RSS at 20 tabs, startup time on a Legacy-tier machine, or a
live-wallpaper frame-time comparison. Treat the "~15%/30%/50%/75% RAM saved"
labels on the Memory Saver levels as descriptions of *how aggressively* tabs are
discarded (2 h / 1 h / 15 min / 2 min idle), not as measured savings. A
`tools/bench.py` harness that drives the browser over a scripted tab set and
samples `VmRSS`/`GetProcessMemoryInfo` would settle this; it is the second
highest-value open task after the unit tests below.

## 9. Testing status

CI builds on Ubuntu 22.04/24.04 (distro Qt, Qt 6.8 LTS and Qt 6.10) and on
Windows Server 2022/2025. Each Linux job runs two headless smoke tests under
Xvfb — one `--classic`, one `--modern` — which is enough to catch a startup
crash, a missing resource or a broken stylesheet, and nothing else. The Windows
jobs build and package but run no test at all.

There are **no unit tests** for `PerformanceProfile`, `MemorySaver`, or
`Theme::metrics()`, and no test target in `CMakeLists.txt`. Adding one is the
highest-value next step in this repository, because the three areas that
matter most are all pure logic that is trivially testable:

| Target | What to assert |
|:---|:---|
| `PerformanceProfile` | `detectTierFromHardware()` maps a synthetic `CpuInfo` to each tier; `chromiumFlags()` never contains `--no-sandbox`; a forced tier beats the saved one; `applyEnvironment()` called twice does not duplicate switches. |
| `MemorySaver::thresholdForLevel` | light/medium/high/ultra map to 2 h/1 h/15 min/2 min and an unknown level falls back to medium. |
| `Theme` | `metrics()` returns a coherent set per style (classic not frameless, status bar visible, zero sidebar); every `@TOKEN@` in the classic sheet is substituted; `styleFromKey()` round-trips with `keyFor()`. |

A smoke test cannot see any of that, which is why these bugs are the kind that
survive releases.
