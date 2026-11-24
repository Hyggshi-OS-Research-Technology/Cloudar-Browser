#pragma once

#include <QString>
#include <QStringList>

// ---------------------------------------------------------------------------
// CPU / low-end hardware compatibility layer.
//
// Cloudar ships one binary per platform, but the machines it runs on range
// from a 2008 Core 2 Duo with 2 GB of RAM to a current 16-core desktop. The
// Chromium engine inside QtWebEngine decides most of that cost on its own,
// but three things are ours to control:
//
//   1. Which Chromium command-line switches we hand it at startup
//      (GPU compositing, raster thread count, renderer process limit,
//      disk cache size, the JS heap cap).
//   2. Which Qt rendering path we ask for (software OpenGL on machines whose
//      drivers are too old to be trusted with the GPU process).
//   3. Whether the UI chrome animates at all — the accent glow on the
//      omnibox, the tab fade-in, the sidebar slide and the live wallpaper
//      are all "nice on a modern box, unusable on a Pentium".
//
// Everything below is resolved once, before QApplication exists, from CPUID,
// the installed RAM and the OS version. It can be overridden by the user with
// the CLOUDAR_PERF_TIER environment variable or the
// performance/cpuTier setting (see docs/CPU_COMPATIBILITY.md).
// ---------------------------------------------------------------------------

enum class CpuTier {
    Legacy,   // pre-2011 CPUs, <3 GB RAM: software rendering, no UI animation
    Low,      // older dual/quad cores, <=6 GB RAM: lean GPU path, minimal motion
    Balanced, // mainstream hardware (default for most machines)
    High      // 8+ cores and 12 GB+ RAM: full effects
};

enum class RenderMode {
    Software,    // GPU process disabled, everything rasterized on the CPU
    Accelerated  // normal Chromium GPU compositing
};

struct CpuInfo {
    QString vendor;
    QString brand;
    int logicalCores = 1;
    int physicalCores = 1;
    quint64 ramMb = 0;
    bool is64Bit = true;
    // x86 feature bits. On a non-x86 host (ARM64) these stay false and
    // `isX86` tells callers not to read anything into that.
    bool isX86 = false;
    bool sse2 = false;
    bool ssse3 = false;
    bool sse41 = false;
    bool sse42 = false;
    bool avx = false;
    bool avx2 = false;
    bool avx512 = false;
    bool fma = false;
};

namespace PerformanceProfile {

// Detected once, cached for the process lifetime.
CpuInfo cpuInfo();

// Hardware-only tier, ignoring any saved or environment override. Exposed so
// the settings page can show "detected: X" next to "in use: Y".
CpuTier detectTierFromHardware();

// Effective tier: forced override if any, otherwise detected from CpuInfo.
CpuTier tier();
void setForcedTier(const QString &tierName); // "auto" | "legacy" | "low" | "balanced" | "high"
QString tierKey(CpuTier tier);
QString tierLabel(CpuTier tier);
bool tierFromKey(const QString &key, CpuTier *out);

RenderMode renderMode();

// Chromium switches for the current tier. Joined with spaces into
// QTWEBENGINE_CHROMIUM_FLAGS by applyEnvironment().
QStringList chromiumFlags();

// Applies QTWEBENGINE_CHROMIUM_FLAGS and the Qt attributes that must be set
// before QApplication is constructed. Safe to call more than once.
//
// Note on ordering: at the moment this runs there is no QApplication yet, so
// QSettings cannot be resolved and the saved tier preference is not visible.
// main() therefore calls this once before QApplication (applying defaults plus
// any environment override) and once after (where the saved preference can be
// applied, along with hardware acceleration and the animation policy).
void applyEnvironment();

// UI motion. Legacy tier disables it by default; the user can force it back
// on from Settings > Performance.
bool animationsEnabled();
void setAnimationsEnabled(bool enabled);

// True when the installed GPU is expected to survive Chromium's GPU process.
// On the Legacy tier we do not even try.
bool gpuTrusted();

// Multi-line, human-readable summary for cloudar://settings and
// cloudar://about. Plain text: the internal pages render it in a <pre>.
QString summary();

// One-line description of the chosen switches, for the about page.
QString activeFlagSummary();

} // namespace PerformanceProfile
