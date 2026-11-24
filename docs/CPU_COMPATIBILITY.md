# CPU Compatibility & Performance Tiers

Cloudar ships one binary per platform, but it has to behave sensibly on
everything from a 2 GB Core 2 Duo to a 16-core desktop. Chromium will happily
try to use features the machine does not have, or spawn a renderer per tab
until RAM runs out; the tier system is how we stop that.

The implementation is [`src/PerformanceProfile.cpp`](../src/PerformanceProfile.cpp).

---

## 1. Detection

At startup, **before `QApplication` exists**, Cloudar reads:

| Signal | Source |
|:---|:---|
| CPU vendor, brand string | CPUID leaves `0` and `0x80000002..4` (x86 only) |
| SSE2 / SSSE3 / SSE4.1 / SSE4.2 | CPUID leaf `1` |
| AVX, FMA | CPUID leaf `1`, then confirmed with `XGETBV` so an AVX-capable CPU running an OS without YMM state does not count as AVX |
| AVX2, AVX-512 | CPUID leaf `7` |
| Physical / logical cores | `QThread::idealThreadCount()` plus the CPUID SMT bit |
| Installed RAM | `GlobalMemoryStatusEx` (Windows), `sysinfo` (Linux), `sysctlbyname` (macOS) |
| 32/64-bit | pointer size |

On ARM64 there is no CPUID; the detector reports the core/RAM counts and marks
x86 feature bits as "not applicable" instead of guessing.

The result is cached for the process lifetime and shown in
**Settings → Performance & Memory → CPU & hardware compatibility**, with the
raw values under "Hardware summary".

## 2. Tier selection

Unless overridden:

| Tier | Chosen when | What it changes |
|:---|:---|:---|
| **Legacy** | RAM < 3 GB, **or** ≤ 2 logical cores without AVX2, **or** no SSSE3 | `--disable-gpu`, no GPU compositing, 1 raster thread, 2 renderer processes, software OpenGL, **no UI animations** |
| **Low** | ≤ 4 logical cores, or ≤ 6 GB RAM | GPU compositing kept, 2 raster threads, 4 renderer processes |
| **Balanced** | everything else (default) | 2 raster threads, 6 renderer processes |
| **High** | ≥ 8 logical cores **and** ≥ 12 GB RAM | 4 raster threads, Chromium's own renderer limit |

Overrides, from lowest to highest priority:

1. `performance/cpuTier` in `QSettings` — **Settings → Performance → Performance tier**
2. `CLOUDAR_PERF_TIER` environment variable
3. `--perf-tier=<tier>` command-line switch (applies for this launch only)

`auto` means "detect". An override made with `--perf-tier` (or
`CLOUDAR_PERF_TIER`) applies to that launch only and is never written to
`QSettings`, so launching from a desktop icon cannot silently change a user's
choice. Only the Settings control persists a tier.

### Why the profile is applied twice at startup

The tier decides `QTWEBENGINE_CHROMIUM_FLAGS`, and Chromium reads that variable
when the *first* web profile is created — which is after `QApplication` exists.
But the tier itself comes from `QSettings`, and `QSettings` cannot resolve a
file until the application name is set. `main()` therefore calls
`applyEnvironment()` once before `QApplication` (hardware detection plus
environment override) and once after command-line parsing (the saved
preference). The second call rebuilds the command line from the inherited value
rather than appending to its own previous output, so no switch appears twice.

When `QTWEBENGINE_CHROMIUM_FLAGS` is already set by a packager or a debugging
session, those switches are kept and ours are appended. `--no-sandbox` is never
added by this code, on any tier.

## 3. What each tier actually passes to Chromium

All tiers get:

```
--disk-cache-size=52428800          # 50 MB cache ceiling
--disable-gpu-shader-disk-cache     # no on-disk shader cache
--autoplay-policy=no-user-gesture-required   # required by the live wallpaper
```

Legacy adds:

```
--disable-gpu --disable-gpu-compositing
--disable-accelerated-2d-canvas --disable-accelerated-video-decode
--disable-features=CanvasOopRasterization,Vulkan
--renderer-process-limit=2 --num-raster-threads=1
--disable-dev-shm-usage             # useful in containers and on small /dev/shm
QT_OPENGL=software, QT_QUICK_BACKEND=software
```

Low / Balanced / High add only `--num-raster-threads` and
`--renderer-process-limit` values from the table above.

Background-tab throttling is left at Chromium's default. There is no
`--disable-background-timer-throttling` in any tier: a background tab that
keeps a full timer budget is the most common reason a laptop fan spins up with
only a browser open.

## 4. The hard floor

QtWebEngine 6.x's Chromium requires **SSE3** as an absolute minimum, and the
official Qt builds assume **SSE4.2**. There is no supported switch to relax
that. Consequently:

- A Pentium III-era or early Athlon 64 machine cannot run this build at all.
- A first-generation Core 2 (SSSE3 but no SSE4) may start and then crash inside
  Chromium; that is a Qt/Chromium limitation, not something Cloudar can patch
  around.
- On such hardware the honest answer is **Cloudar Legacy** (Qt 5.15 WebEngine),
  with the security caveats in [`PLATFORM_MATRIX.md`](PLATFORM_MATRIX.md) §5.

The `sse2/ssse3/sse41/sse42` fields are reported in Settings so a support
request can be answered with data instead of guesswork.

## 5. Why the GPU is treated as hostile by default on old machines

Chromium's GPU process is the single most common source of "the browser opens a
white page and dies" on pre-2012 hardware. The Legacy tier therefore refuses to
create a GPU process at all and uses Qt's software OpenGL rasterizer. This is
slower for scrolling on a machine with a working discrete GPU, which is exactly
why it is opt-out-able:

- **Settings → Performance → Hardware acceleration** (requires restart), or
- `CLOUDAR_FORCE_SOFTWARE_RENDER=1` / `=0`.

## 6. UI animation

`PerformanceProfile::animationsEnabled()` controls:

- the address-bar accent glow (a `QGraphicsDropShadowEffect`, which forces the
  software compositing path),
- the tab fade-in on new tabs,
- the sidebar width slide,
- the progress-bar value tween.

Default: **off on Legacy**, on elsewhere. Also overridable with
`CLOUDAR_UI_ANIMATIONS=0` or **Settings → Performance → Interface animations**.
The classic (Chrome 2009) chrome has no glow or fade regardless, because that
interface never had them.

## 7. Memory Saver interaction

The tier chooses engine flags; [`MemorySaver`](../src/MemorySaver.h) decides
whether background renderers stay alive. On a Legacy-tier machine with 2 GB of
RAM the sensible combination is **Legacy tier + Medium or High Memory Saver**,
which is what the settings page suggests. Note that a slept tab reloads from
the network when re-activated — on a slow machine that is a visible delay, so
the Ultra level is deliberately not the default.

## 8. Measuring, rather than guessing

```bash
# Which tier was chosen, and why
CLOUDAR_PERF_TIER=auto ./build/cloudar-browser --version   # logs on first window
# Raw CPUID feature bits
grep -m1 flags /proc/cpuinfo
# Effective Chromium switches
grep -a QTWEBENGINE_CHROMIUM_FLAGS /proc/$(pgrep -f cloudar-browser | head -1)/environ | tr '\0' '\n'
```

`cloudar://settings/performance` shows the same information without a terminal.
