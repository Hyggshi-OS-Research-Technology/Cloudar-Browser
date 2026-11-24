# Platform Support Matrix

What Cloudar Browser actually builds and runs on, what it does not, and the
branch/backend that would be required for each gap. This supersedes any older
claim that one binary covers Windows 7 through 11, Debian 11 through 14 and
Android 3.4 through 16.

Three products are proposed, because one Qt 6 codebase cannot span that range:

| Product | Stack | Targets | Status |
|:---|:---|:---|:---|
| **Cloudar Desktop** | C++17, Qt 6.5 LTS / 6.8 LTS / 6.10+, QtWebEngine | Windows 10/11 (x64, arm64 tech preview), modern Linux (x64, arm64) | **this branch** |
| **Cloudar Legacy** | C++17, Qt 5.15 LTS, QtWebEngine 5.15 (Chromium 87) | Windows 7 SP1 / 8 / 8.1 (x86, x64), Debian 10/11, Ubuntu 18.04/20.04, Fedora 32-ish | separate branch, frozen, security caveats below |
| **Cloudar Mobile** | Kotlin/Java, `android.webkit.WebView` (+ optional Qt 6.5 shell for API 26+) | Android 5.0 (API 21) through current | separate branch, not in this repo |

---

## 1. Windows

| OS | Cloudar Desktop (Qt 6) | Cloudar Legacy (Qt 5.15) | Notes |
|:---|:---|:---|:---|
| Windows 11 (x64, arm64) | ✅ supported | n/a | Primary modern target. MSVC 2022 or MinGW 11. |
| Windows 10 1809+ (x64) | ✅ supported | n/a | Minimum for all Qt 6 releases. |
| Windows 10 1507–1709 | ⚠️ unsupported by Qt 6 | ✅ | Only relevant on frozen LTSC images. |
| Windows 8.1 / 8 | ❌ cannot run Qt 6 | ✅ Qt 5.15 LTS (EOL) | Chromium 87-era engine. See §5. |
| Windows 7 SP1 | ❌ cannot run Qt 6 | ✅ Qt 5.15 LTS (EOL) | Needs the UCRT update (KB2999226). |
| Windows XP / Vista | ❌ | ❌ | Out of scope. |

**Why Qt 6 cannot target Windows 7/8.1:** Qt 6 requires Windows 10 APIs at both
build and run time, Chromium dropped Windows 7/8.1 support in 2023, and Qt has
stated Windows 7/8.x will not be supported in Qt 6. Recompiling this tree with
an older compiler does not change that.

**Windows build outline (Desktop):**

```powershell
# with Qt 6.8 LTS installed via the Qt online installer
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
windeployqt --release build/cloudar-browser.exe
```

Windows-specific packaging (installer, code signing, AppUserModelID) is not
present in this repository yet; `windeployqt` output is the current artefact.

## 2. Linux

| Distribution | Qt 6 WebEngine available? | Cloudar Desktop | Cloudar Legacy |
|:---|:---|:---|:---|
| Debian 13 (trixie) | ✅ Qt 6.8 | ✅ | n/a |
| Debian 12 (bookworm) | ✅ Qt 6.4 | ✅ | n/a |
| Debian 11 (bullseye) | ❌ no `qt6-webengine-dev` | ⚠️ only with a self-built Qt (6.2/6.4) | ✅ Qt 5.15 from backports |
| Debian 10 (buster) | ❌ | ❌ | ✅ |
| Ubuntu 26.04 / 25.10 / 24.04 LTS / 24.10 | ✅ Qt 6.4–6.9 | ✅ | n/a |
| Ubuntu 22.04 LTS | ✅ Qt 6.2 | ✅ | n/a |
| Ubuntu 20.04 LTS | ❌ no `qt6-webengine-dev` | ⚠️ Qt online installer build only | ✅ |
| Fedora 41+ | ✅ | ✅ | n/a |
| Arch / Manjaro | ✅ rolling | ✅ | n/a |

Notes:

- Qt 6.2 is the floor for the C++ code (the `QWebEnginePermission` path is
  guarded by `QT_VERSION >= 6.7`; older versions use the deprecated
  `featurePermissionRequested` signal). Qt 6.2 LTS is out of general support,
  so the recommended minimum is **Qt 6.5 LTS**.
- Debian 11's GCC 10 can build Qt 6.2 but not Qt 6.4+ without a newer toolchain;
  there is no distribution package for WebEngine, so it is not a supported
  target for the shipped binary.
- ARM64 Linux builds work but are untested by CI. The performance tier detector
  handles them by treating "no x86 feature bits" as its own case rather than
  assuming SSE.

## 3. Android (the hardest target)

| Android version | API | Path |
|:---|:---|:---|
| 1.5 – 3.2 | 3 – 13 | ❌ no path. No maintained WebView. |
| 4.0 – 4.4 | 14 – 19 | ❌ `WebView` from Play Store stops at API 19 + Google no longer ships updates; TLS to most of the web fails. |
| 5.0 – 7.1 | 21 – 25 | ✅ **Cloudar Mobile** only (system `WebView` is updatable through Play). Not reachable with Qt 6. |
| 8.0 – 8.1 | 26 – 27 | ✅ Cloudar Mobile; Qt 6.5 LTS also possible (min API 26) |
| 9 – 16 | 28 – 36 | ✅ Cloudar Mobile; Qt 6.8/6.10 shell possible (Qt 6.8+ min API 28) |

**Why Android 3.4 (API 10) is not achievable:** it predates the Chromium
WebView entirely (the Android Browser used WebKit), its TLS stack cannot
negotiate modern HTTPS, and no current engine targets it. Supporting "Android
3.4 to latest" in one binary is not a packaging problem; it is a different
browser. The realistic floor for a modern Cloudar on Android is **API 21**,
and only through the platform WebView.

**Why QtWebEngine cannot be reused:** QtWebEngine has no mobile target. Qt for
Android ships WebEngine only on desktop; on Android you get `QWebView`
equivalents in Qt 5 (deprecated) or nothing in Qt 6. A mobile Cloudar has to
own its WebView integration.

## 4. Chromium versions bundled with QtWebEngine

| Qt | Chromium baseline | Notes |
|:---|:---|:---|
| 6.5 LTS | 108 → 118 (updated through the LTS cycle) | LTS, still receiving backports |
| 6.6 | 112 | |
| 6.7 | 118 | |
| 6.8 LTS | 122 → 130+ | LTS, receives security backports for 3 years |
| 6.9 | 130 | |
| 6.10 | 134 | adds experimental WebExtension support |

The runtime value is authoritative: `cloudar://about` prints
`qWebEngineChromiumVersion()`. Treat any branch older than the current stable
minus one major as "legacy profile": acceptable for internal pages, not for
untrusted browsing.

## 5. What "legacy" costs, stated plainly

A legacy build runs Chromium 87 (Qt 5.15 LTS WebEngine, last patched by Qt in
2023). That engine:

- cannot render a growing number of modern sites correctly (CSS `:has()`
  support is partial, several 2024+ JS APIs are missing),
- will not receive upstream security fixes,
- will fail on sites that require TLS 1.3 + modern cipher suites or newer
  certificate extensions.

Shipping it is a deliberate trade: a usable browser on hardware that cannot run
anything else, clearly labelled as unsupported for sensitive browsing
(banking, email, anything authenticated). It must never be the default download
for a machine that can run the desktop build.

## 6. CPU / GPU floor (Desktop)

| Requirement | Detail |
|:---|:---|
| CPU | x86-64 with SSE3 minimum; SSE4.2 assumed by official Qt builds. ARM64 supported where Qt ships it. |
| RAM | 2 GB to start (Legacy tier), 4 GB recommended, 8 GB comfortable |
| GPU | optional: the Legacy tier runs entirely on the software rasterizer |
| OS | Windows 10 1809+ / a Linux with Qt 6 WebEngine ≥ 6.5 |

See [`CPU_COMPATIBILITY.md`](CPU_COMPATIBILITY.md) for how the tier is chosen
and what each tier changes.
