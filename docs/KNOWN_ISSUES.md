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

Web notifications and web shortcuts were on this list as "not implemented".
Both now exist, incompletely and on purpose: see §12 (the cards are drawn by
Cloudar, not by the operating system) and §13 (a shortcut is a saved URL, not an
installed app).

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

There are now two test targets, built with `-DCLOUDAR_BUILD_TESTS=ON`:

* `tests/tst_localfileservice.cpp` — `LocalFileService`, because it decides which
  directories a `cloudar://` page can make the browser enumerate and that is a
  security boundary (see `docs/FILE_PROTOCOL_SECURITY.md`);
* `tests/tst_webshortcutstore.cpp` — `WebShortcutStore`, because the menu it
  feeds has invariants (one site one shortcut, ids surviving a rename) that break
  silently.

Everything else is still untested. `PerformanceProfile`, `MemorySaver` and
`Theme::metrics()` remain the highest-value additions, because all three are pure
logic and trivially testable:

| Target | What to assert | Status |
|:---|:---|:---|
| `PerformanceProfile` | `detectTierFromHardware()` maps a synthetic `CpuInfo` to each tier; `chromiumFlags()` never contains `--no-sandbox`; a forced tier beats the saved one; `applyEnvironment()` called twice does not duplicate switches. | not written |
| `MemorySaver::thresholdForLevel` | light/medium/high/ultra map to 2 h/1 h/15 min/2 min and an unknown level falls back to medium. | not written |
| `Theme` | `metrics()` returns a coherent set per style (classic not frameless, status bar visible, zero sidebar); every `@TOKEN@` in the classic sheet is substituted; `styleFromKey()` round-trips with `keyFor()`. | not written |
| `LocalFileService` | path confinement, `..`/symlink/sibling-prefix escapes, `~/` expansion, path-vs-search resolution. | **written** |
| `WebShortcutStore` | one site one shortcut, id survives a rename, trailing slash/fragment normalisation, file:// and cloudar:// refused, corrupt storage reads as empty. | **written** |

A smoke test cannot see any of that, which is why these bugs are the kind that
survive releases.

## 10. Interface language coverage

The interface language setting is implemented end to end (see
`docs/LANGUAGE.md`) and applies immediately. What it does **not** cover today:

* **Settings page body copy.** Section names, page headings, card titles and the
  Language card are translated; the long explanatory paragraphs under each
  control — the most text-heavy part of the product — are still English only.
  This is the largest remaining piece of translation work and it is deliberately
  not claimed as done.
* **Chromium-owned UI**: the DevTools window, the built-in PDF viewer and form
  validation bubbles. Those strings live in QtWebEngine's resource tree; this
  repository ships no `.pak` replacements for them.
* **`--help` / `--version` output**, which `src/main.cpp` prints before
  `QApplication` exists so that it works headless.
* **`title` attributes on a few icon-only buttons** whose tooltip is set from a
  literal in a file this PR did not touch; `tools/check-i18n.js` only enforces the
  strings it can see, and those are plain `setToolTip("…")` calls, not `tr()`.

`lang/{ja,zh}.json` were 13 keys short of `en.json` before this work; all
dictionaries now carry identical key sets, which `node tools/check-i18n.js`
enforces on every push.

## 11. Tearing a tab off into its own window

Dragging a tab out of the vertical sidebar over empty space opens it in a new
window, in the same spirit as Chrome's tear-off. The gesture is a sidebar
feature: the horizontal tab strip (the only surface in the classic chrome) keeps
`QTabWidget`'s own drag-to-reorder and has no tear-off. What the new window is
**not** is a moved tab:

* **The page is reopened, not relocated.** Moving a live `QWebEngineView` into
  another top-level window re-parents its Chromium rendering delegate, and the
  first frame after such a move is a reliable way to get a blank view. The new
  window loads the URL instead, so **in-page state is lost**: form input, scroll
  position, a playing video's position.
* **The tab's back/forward list is lost** for the same reason. `Ctrl+Shift+T`
  in the old window reopens the closed tab with a list that still only knows
  the URL.
* **The new window has its own `QWebEngineProfile`**, exactly like
  **File -> New Window**. Cookies and logins therefore do not carry over: a
  site you are signed into in one window is signed out in the other. Sharing the
  profile would mean handing one window's profile to another and skipping every
  profile-level setup step (scheme handlers, the ad-block interceptor, the
  engine attributes) for the second window, which is a larger change than the
  gesture warrants.
* **It does not touch the saved session.** A window holding one torn-off tab
  neither restores the session on start nor writes it on close, so closing the
  main window still saves the tabs you were actually working in.
* **Slept tabs tear off too.** Memory Saver replaced the view with a placeholder,
  so there is no page to snapshot; the remembered URL is what opens in the new
  window.

Reordering tabs *inside* the sidebar has none of these limitations: it moves the
real tab, which keeps its history, its page state and its cookies, because both
surfaces are one call to `QTabBar::moveTab()`.

## 12. Notifications are real, but they are not OS notifications

The roadmap line for this item said "declare unsupported", on the assumption that
QtWebEngine had no notification API. That assumption was wrong:
`QWebEngineProfile::setNotificationPresenter()` has existed since Qt 5.13
(`QWebEngineNotification` since 2019), so web notifications are **implemented**
(`src/NotificationPresenter.*`, `src/NotificationToast.*`) rather than disclaimed.

What works:

* A site the user allowed (the normal permission prompt, stored by
  `PermissionStore`) gets a card in the corner of the window that owns the page.
* Clicking the card runs the page's `click()` handler and raises the tab; the
  dismiss button and the six-second timeout run `close()`, so the page is never
  left waiting for an answer that will not come.
* At most three at a time; the oldest is evicted.
* A denied site is dropped silently, and so is one that never asked for
  permission — the page cannot distinguish "denied" from "nobody was watching".

What does **not** work, and is not claimed:

* **No operating-system delivery.** There is no libnotify / Windows Toast XML /
  macOS path in this build, so nothing appears when the Cloudar window is closed,
  and nothing is queued for later. There is no notification centre.
* **No action buttons, no `requireInteraction`, no `silent`/`renotify`, no
  `badge`.** The card shows title, message, site icon and origin, which is all
  `QWebEngineNotification` exposes.
* **The `tag` is not used to replace a toast.** The engine's own `matches()`
  logic already collapses a notification the page means as an update of another.
* **Full-screen and split-view pages:** a toast is positioned in the window, so
  in full screen it appears over the page rather than in an OS overlay. In split
  view it is anchored to the window, not to the pane that sent it.

## 13. Web shortcuts are not PWA installation

`Tools ▸ Web shortcuts` (classic) and the ☰ menu's **Web shortcuts** submenu save
a name and a URL and open that page in a chrome-less window
(`MainWindow::createAppWindow`, `src/WebShortcutStore.*`). It is honest to call
this "the thing Chrome calls *Create shortcut*", and not honest to call it an
installed app:

* **Nothing is installed.** No `manifest.json` is fetched, no service worker is
  registered, no assets are cached. The page still needs the network on every
  launch, so a shortcut to an app that would normally work offline does not.
* **No icon is taken from the site.** The window and any future launcher entry
  use the browser's own icon, because the manifest's icons are exactly the part
  that would require the install step above.
* **No Start-menu or desktop entry is written.** There is no installer, and
  writing `.desktop`/`.lnk` files from a browser without a per-platform
  registration path is how a browser ends up shipping a half-working "install"
  button. `cloudar-browser --app <url>` and `--app-id <id>` exist so a launcher
  someone writes by hand works.
* **The window is one page.** Links that ask for a new window open in a normal
  browser window, not inside the app window: there is no tab strip there, so a
  popup that targeted it would be unreachable.
* **Only http/https.** `file://` is refused on purpose (a chrome-less window on
  a local file would be a second route to the filesystem that bypasses the
  confinement in `FILE_PROTOCOL_SECURITY.md`), and so is `cloudar://`.

Shortcuts are stored in `QSettings` under `browser/webShortcuts`, deliberately not
in the renderer's storage partition, which is cleared with browsing data.
