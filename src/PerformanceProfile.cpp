#include "PerformanceProfile.h"

#include <QSettings>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QStringList>
#include <QtGlobal>
#include <QThread>
#include <QDebug>

#include <algorithm>
#include <cstring>

#if defined(Q_OS_WIN)
#  include <windows.h>
#elif defined(Q_OS_LINUX)
#  include <sys/sysinfo.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <sys/sysctl.h>
#  include <unistd.h>
#endif

#if defined(Q_PROCESSOR_X86) || defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#  define CLOUDAR_HAVE_CPUID 1
#  if defined(_MSC_VER)
#    include <intrin.h>
#  else
#    include <cpuid.h>
#  endif
#endif

namespace {

// --- CPUID plumbing -------------------------------------------------------
// Kept in one place so the rest of the file never has to care whether we are
// on MSVC's intrinsics, GCC's <cpuid.h>, or a non-x86 target with no CPUID
// at all (ARM64 Windows/Linux, Apple silicon).

struct CpuidResult {
    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;
};

CpuidResult cpuid(unsigned int leaf, unsigned int subleaf = 0)
{
    CpuidResult r;
#ifdef CLOUDAR_HAVE_CPUID
#  if defined(_MSC_VER)
    int out[4] = {0, 0, 0, 0};
    __cpuidex(out, static_cast<int>(leaf), static_cast<int>(subleaf));
    r.eax = static_cast<unsigned int>(out[0]);
    r.ebx = static_cast<unsigned int>(out[1]);
    r.ecx = static_cast<unsigned int>(out[2]);
    r.edx = static_cast<unsigned int>(out[3]);
#  else
    unsigned int a = 0, b = 0, c = 0, d = 0;
    __cpuid_count(leaf, subleaf, a, b, c, d);
    r.eax = a;
    r.ebx = b;
    r.ecx = c;
    r.edx = d;
#  endif
#else
    Q_UNUSED(leaf);
    Q_UNUSED(subleaf);
#endif
    return r;
}

QString cpuBrandString()
{
#ifdef CLOUDAR_HAVE_CPUID
    char brand[49] = {0};
    for (unsigned int leaf = 0x80000002u; leaf <= 0x80000004u; ++leaf) {
        const CpuidResult r = cpuid(leaf);
        std::memcpy(brand + (leaf - 0x80000002u) * 16, &r.eax, 4);
        std::memcpy(brand + (leaf - 0x80000002u) * 16 + 4, &r.ebx, 4);
        std::memcpy(brand + (leaf - 0x80000002u) * 16 + 8, &r.ecx, 4);
        std::memcpy(brand + (leaf - 0x80000002u) * 16 + 12, &r.edx, 4);
    }
    QString brand_str = QString::fromLatin1(brand).simplified();
    if (!brand_str.isEmpty())
        return brand_str;
#endif
    // Non-x86 or a hypervisor that hides the brand: fall back to the
    // platform's own naming.
#if defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("ARM64 processor");
#else
    return QStringLiteral("Unknown processor");
#endif
}

QString cpuVendor()
{
#ifdef CLOUDAR_HAVE_CPUID
    const CpuidResult r = cpuid(0);
    char vendor[13] = {0};
    std::memcpy(vendor, &r.ebx, 4);
    std::memcpy(vendor + 4, &r.edx, 4);
    std::memcpy(vendor + 8, &r.ecx, 4);
    return QString::fromLatin1(vendor).trimmed();
#else
    return QStringLiteral("unknown");
#endif
}

quint64 totalRamMb()
{
#if defined(Q_OS_WIN)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        return static_cast<quint64>(status.ullTotalPhys / (1024ull * 1024ull));
#elif defined(Q_OS_LINUX)
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        const quint64 mem = static_cast<quint64>(info.totalram) * info.mem_unit;
        return mem / (1024ull * 1024ull);
    }
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    quint64 bytes = 0;
    size_t len = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &len, nullptr, 0) == 0)
        return bytes / (1024ull * 1024ull);
#endif
    return 0;
}

// Physical core count without dragging in a whole topology library: x86
// exposes an SMT bit, and logical/2 is the correct answer on every CPU that
// sets it except a handful of oddities where it is still a safer guess than
// assuming SMT means double the real cores.
int physicalCoreCount(int logical, bool smt)
{
    if (logical <= 1)
        return logical;
    if (!smt)
        return logical;
    return (std::max)(1, logical / 2);
}

PerformanceProfile::CpuInfo detectCpu()
{
    PerformanceProfile::CpuInfo info;
    info.vendor = cpuVendor();
    info.brand = cpuBrandString();
    // Parenthesised: <windows.h> defines a `max` macro, which turns
    // std::max(1, n) into a syntax error under MSVC.
    info.logicalCores = (std::max)(1, QThread::idealThreadCount());
    info.ramMb = totalRamMb();
    info.is64Bit = (sizeof(void *) == 8);

#ifdef CLOUDAR_HAVE_CPUID
    info.isX86 = true;
    const CpuidResult leaf1 = cpuid(1);
    info.sse2 = (leaf1.edx >> 26) & 1;
    info.ssse3 = (leaf1.ecx >> 9) & 1;
    info.sse41 = (leaf1.ecx >> 19) & 1;
    info.sse42 = (leaf1.ecx >> 20) & 1;
    info.avx = (leaf1.ecx >> 28) & 1;
    info.fma = (leaf1.ecx >> 12) & 1;
    const bool smt = (leaf1.edx >> 28) & 1;

    // AVX state must be enabled by the OS (XGETBV) before the bit means
    // anything: a CPU can report AVX while a 2009-era OS build leaves the
    // YMM state unsaved.
    if (info.avx) {
        const CpuidResult osxsave = cpuid(1);
        const bool osxsave_bit = (osxsave.ecx >> 27) & 1;
        if (!osxsave_bit) {
            info.avx = false;
            info.fma = false;
        } else {
#if defined(_MSC_VER)
            const unsigned long long xcr0 = _xgetbv(0);
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
            unsigned int eaxX = 0, edxX = 0;
            __asm__ __volatile__("xgetbv" : "=a"(eaxX), "=d"(edxX) : "c"(0));
            const unsigned long long xcr0 =
                (static_cast<unsigned long long>(edxX) << 32) | eaxX;
#else
            const unsigned long long xcr0 = 0;
#endif
            if ((xcr0 & 0x6) != 0x6) {
                info.avx = false;
                info.fma = false;
            }
        }
    }

    const CpuidResult maxLeaf = cpuid(0);
    if (maxLeaf.eax >= 7) {
        const CpuidResult leaf7 = cpuid(7, 0);
        info.avx2 = info.avx && ((leaf7.ebx >> 5) & 1);
        info.avx512 = info.avx && ((leaf7.ebx >> 16) & 1);
    }

    info.physicalCores = physicalCoreCount(info.logicalCores, smt);
#else
    info.isX86 = false;
    info.physicalCores = info.logicalCores;
#endif
    return info;
}

QString readEnv(const char *name)
{
    const QByteArray value = qgetenv(name);
    return QString::fromLocal8Bit(value).trimmed();
}

bool envFlagIsTruthy(const char *name, bool fallback)
{
    const QString value = readEnv(name).toLower();
    if (value.isEmpty())
        return fallback;
    return value != "0" && value != "false" && value != "no" && value != "off";
}

} // namespace

namespace PerformanceProfile {

namespace {

// QSettings needs QCoreApplication's organization/application name to know
// *which* settings file to open. applyEnvironment() runs before QApplication is
// constructed, so during that window we cannot read the saved preferences at
// all -- and a QSettings built with an empty organization name would silently
// read the wrong file and then be cached. These three helpers read QSettings
// only when an application name exists, and fall back to the defaults (which is
// exactly the state applyEnvironment() needs) otherwise.
bool settingsAvailable()
{
    return QCoreApplication::instance() != nullptr
           && !QCoreApplication::applicationName().isEmpty();
}

QString settingsString(const char *key, const QString &fallback = QString())
{
    if (!settingsAvailable())
        return fallback;
    return QSettings().value(QString::fromLatin1(key), fallback).toString();
}

bool settingsBool(const char *key, bool fallback)
{
    if (!settingsAvailable())
        return fallback;
    return QSettings().value(QString::fromLatin1(key), fallback).toBool();
}

} // namespace

CpuInfo cpuInfo()
{
    static const CpuInfo info = detectCpu();
    return info;
}

QString tierKey(CpuTier tier)
{
    switch (tier) {
    case CpuTier::Legacy: return QStringLiteral("legacy");
    case CpuTier::Low: return QStringLiteral("low");
    case CpuTier::Balanced: return QStringLiteral("balanced");
    case CpuTier::High: return QStringLiteral("high");
    }
    return QStringLiteral("balanced");
}

QString tierLabel(CpuTier tier)
{
    switch (tier) {
    case CpuTier::Legacy: return QStringLiteral("Legacy (minimum CPU/GPU load)");
    case CpuTier::Low: return QStringLiteral("Low (older CPU, reduced effects)");
    case CpuTier::Balanced: return QStringLiteral("Balanced (recommended)");
    case CpuTier::High: return QStringLiteral("High (full effects)");
    }
    return QStringLiteral("Balanced");
}

bool tierFromKey(const QString &key, CpuTier *out)
{
    const QString normalized = key.trimmed().toLower();
    CpuTier parsed;
    if (normalized == "legacy") parsed = CpuTier::Legacy;
    else if (normalized == "low") parsed = CpuTier::Low;
    else if (normalized == "balanced") parsed = CpuTier::Balanced;
    else if (normalized == "high") parsed = CpuTier::High;
    else return false;

    if (out)
        *out = parsed;
    return true;
}

void setForcedTier(const QString &tierName)
{
    if (!settingsAvailable())
        return; // cannot persist without an application identity
    const QString normalized = tierName.trimmed().toLower();
    QSettings settings;
    if (normalized.isEmpty() || normalized == "auto")
        settings.remove("performance/cpuTier");
    else
        settings.setValue("performance/cpuTier", normalized);
}

CpuTier detectTierFromHardware()
{
    const CpuInfo info = cpuInfo();

    // Legacy: the class of hardware Chromium itself has quietly stopped
    // optimising for. No AVX2, few cores, little RAM.
    if (info.ramMb > 0 && info.ramMb < 3072)
        return CpuTier::Legacy;
    if (info.logicalCores <= 2 && !info.avx2)
        return CpuTier::Legacy;
    if (info.isX86 && !info.ssse3)
        return CpuTier::Legacy;

    if (info.logicalCores <= 4 || (info.ramMb > 0 && info.ramMb <= 6144))
        return CpuTier::Low;

    if (info.logicalCores >= 8 && info.ramMb >= 12288)
        return CpuTier::High;

    return CpuTier::Balanced;
}

CpuTier tier()
{
    // Priority: environment override (useful for packaging/CI and for a user
    // who cannot reach Settings on a machine that will not start), then the
    // saved preference, then autodetection.
    const QString env = readEnv("CLOUDAR_PERF_TIER");
    CpuTier forced;
    if (!env.isEmpty() && env.toLower() != "auto" && tierFromKey(env, &forced))
        return forced;

    const QString saved = settingsString("performance/cpuTier");
    if (!saved.isEmpty() && saved.toLower() != "auto" && tierFromKey(saved, &forced))
        return forced;

    return detectTierFromHardware();
}

RenderMode renderMode()
{
    if (envFlagIsTruthy("CLOUDAR_FORCE_SOFTWARE_RENDER", false))
        return RenderMode::Software;
    if (tier() == CpuTier::Legacy)
        return RenderMode::Software;
    if (!settingsBool("performance/hardwareAcceleration", true))
        return RenderMode::Software;
    return RenderMode::Accelerated;
}

bool gpuTrusted()
{
    return renderMode() == RenderMode::Accelerated;
}

bool animationsEnabled()
{
    // Explicit user choice wins; otherwise the Legacy tier starts still.
    if (settingsAvailable()) {
        QSettings settings;
        if (settings.contains("performance/animations"))
            return settings.value("performance/animations").toBool();
    }

    if (!envFlagIsTruthy("CLOUDAR_UI_ANIMATIONS", true))
        return false;

    return tier() != CpuTier::Legacy;
}

void setAnimationsEnabled(bool enabled)
{
    if (!settingsAvailable())
        return; // cannot persist without an application identity
    QSettings().setValue("performance/animations", enabled);
}

QStringList chromiumFlags()
{
    QStringList flags;

    // --- Engine cost controls, applied on every tier -----------------------
    // Small disk cache: this is a "light" browser and a 50 MB cache keeps
    // long-running sessions from growing without bound.
    flags << "--disk-cache-size=52428800";
    flags << "--disable-gpu-shader-disk-cache";
    // Keep Chromium's default background-tab throttling in place (do not
    // pass --disable-background-timer-throttling): a background tab that
    // still burns a full timer budget is the single most common cause of a
    // laptop fan spinning up with a browser open.

    const CpuTier current = tier();

    switch (current) {
    case CpuTier::Legacy:
        // Software rasterization only: no GPU process to crash the browser
        // on a broken 2009 driver, and no attempt to decode video in
        // hardware that cannot do it anyway.
        flags << "--disable-gpu";
        flags << "--disable-gpu-compositing";
        flags << "--disable-accelerated-2d-canvas";
        flags << "--disable-accelerated-video-decode";
        flags << "--disable-features=CanvasOopRasterization,Vulkan";
        flags << "--renderer-process-limit=2";
        flags << "--num-raster-threads=1";
        flags << "--disable-dev-shm-usage";
        break;

    case CpuTier::Low:
        flags << "--renderer-process-limit=4";
        flags << "--num-raster-threads=2";
        break;

    case CpuTier::Balanced:
        flags << "--renderer-process-limit=6";
        flags << "--num-raster-threads=2";
        break;

    case CpuTier::High:
        flags << "--num-raster-threads=4";
        break;
    }

    // Referenced by the New Tab page's live-wallpaper feature (see
    // main.cpp history: autoplay must be allowed for a video background).
    flags << "--autoplay-policy=no-user-gesture-required";

    // Note on CPU floors: QtWebEngine 6.x's bundled Chromium requires SSE3
    // at minimum, and the official Qt builds assume SSE4.2-era hardware.
    // There is no supported switch to relax that, so the hard floor is
    // documented in docs/CPU_COMPATIBILITY.md instead of being papered over
    // here. Everything above the floor is what the tier system controls.

    return flags;
}

void applyEnvironment()
{
    // Must be set before QApplication is constructed so QWebEngine picks it up
    // when it initialises Chromium on first page load.
    //
    // A packager or a debugging session may already have set extra switches
    // (--enable-logging, a proxy, a remote-debugging port). Those are kept and
    // ours are appended. The first call records the inherited value so the
    // second call rebuilds from that baseline instead of appending to its own
    // output and ending up with every switch twice.
    static const QString inherited = qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS").trimmed();

    QStringList switches = chromiumFlags();
    if (!inherited.isEmpty()) {
        for (const QString &existing : inherited.split(' ', Qt::SkipEmptyParts)) {
            if (!switches.contains(existing))
                switches.prepend(existing);
        }
    }
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", switches.join(' ').toUtf8());

    if (renderMode() == RenderMode::Software) {
        // Qt's own raster fallback: force the software OpenGL implementation
        // for the widget layer too, so a machine that cannot create a GL
        // context does not fail to start at all.
        if (qEnvironmentVariableIsEmpty("QT_OPENGL"))
            qputenv("QT_OPENGL", "software");
        if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND"))
            qputenv("QT_QUICK_BACKEND", "software");
    }

    // We deliberately never set QTWEBENGINE_DISABLE_SANDBOX or --no-sandbox
    // here, not even on the Legacy tier: a machine too slow for a GPU
    // process is not a reason to remove the renderer's security boundary.
    // If a specific distribution cannot start the sandbox, that belongs in
    // its own packaging notes, not in the shared code path.
}

QString summary()
{
    const CpuInfo info = cpuInfo();
    const QStringList featureList = {
        info.sse42 ? QStringLiteral("SSE4.2") : QString(),
        info.avx ? QStringLiteral("AVX") : QString(),
        info.avx2 ? QStringLiteral("AVX2") : QString(),
        info.avx512 ? QStringLiteral("AVX-512") : QString()
    };

    QStringList features;
    for (const QString &f : featureList) {
        if (!f.isEmpty())
            features << f;
    }
    if (features.isEmpty())
        features << QStringLiteral("baseline only");

    QString out;
    out += QStringLiteral("CPU: %1\n").arg(info.brand);
    out += QStringLiteral("Vendor: %1\n").arg(info.vendor);
    out += QStringLiteral("Cores: %1 physical / %2 logical\n")
               .arg(info.physicalCores)
               .arg(info.logicalCores);
    out += QStringLiteral("Architecture: %1\n")
               .arg(info.is64Bit ? QStringLiteral("64-bit") : QStringLiteral("32-bit"));
    if (info.ramMb > 0)
        out += QStringLiteral("Memory: %1 MB\n").arg(info.ramMb);
    out += QStringLiteral("Instructions: %1\n").arg(features.join(QStringLiteral(", ")));
    out += QStringLiteral("Tier: %1\n").arg(tierLabel(tier()));
    out += QStringLiteral("Rendering: %1\n")
               .arg(renderMode() == RenderMode::Software
                        ? QStringLiteral("software rasterizer")
                        : QStringLiteral("GPU compositing"));
    out += QStringLiteral("UI animations: %1")
               .arg(animationsEnabled() ? QStringLiteral("on") : QStringLiteral("off"));
    return out;
}

QString activeFlagSummary()
{
    return chromiumFlags().join(' ');
}

} // namespace PerformanceProfile
