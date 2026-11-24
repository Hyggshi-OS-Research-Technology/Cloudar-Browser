#include "PermissionStore.h"

#include <QSettings>
#include <QStringList>
#include <QWebEnginePage>
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#include <QWebEnginePermission>
#endif

namespace PermissionStore {
namespace {

const char *kDecisionPrefix = "permissions/";

// The full vocabulary, in the order the Qt 6.7+ enum declares it. Qt 6.2's
// QWebEnginePage::Feature has no clipboard or local-font entries, so the list
// is shared and the mapping functions below simply report -1 for those.
const QStringList &capabilities()
{
    static const QStringList list = {
        QStringLiteral("camera"),
        QStringLiteral("microphone"),
        QStringLiteral("camera and microphone"),
        QStringLiteral("location"),
        QStringLiteral("notifications"),
        QStringLiteral("pointer lock"),
        QStringLiteral("screen sharing"),
        QStringLiteral("screen and audio sharing"),
        QStringLiteral("clipboard"),
        QStringLiteral("local fonts"),
    };
    return list;
}

QString settingsKey(const QString &host, const QString &capability)
{
    return QLatin1String(kDecisionPrefix) + host + '/' + capability;
}

} // namespace

QString capabilityForEngineType(int engineType)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    using Type = QWebEnginePermission::PermissionType;
    switch (Type(engineType)) {
    case Type::MediaAudioCapture: return QStringLiteral("microphone");
    case Type::MediaVideoCapture: return QStringLiteral("camera");
    case Type::MediaAudioVideoCapture: return QStringLiteral("camera and microphone");
    case Type::Geolocation: return QStringLiteral("location");
    case Type::Notifications: return QStringLiteral("notifications");
    case Type::MouseLock: return QStringLiteral("pointer lock");
    case Type::DesktopVideoCapture: return QStringLiteral("screen sharing");
    case Type::DesktopAudioVideoCapture: return QStringLiteral("screen and audio sharing");
    case Type::ClipboardReadWrite: return QStringLiteral("clipboard");
    case Type::LocalFontsAccess: return QStringLiteral("local fonts");
    default: return QString();
    }
#else
    using Feature = QWebEnginePage::Feature;
    switch (Feature(engineType)) {
    case Feature::MediaAudioCapture: return QStringLiteral("microphone");
    case Feature::MediaVideoCapture: return QStringLiteral("camera");
    case Feature::MediaAudioVideoCapture: return QStringLiteral("camera and microphone");
    case Feature::Geolocation: return QStringLiteral("location");
    case Feature::Notifications: return QStringLiteral("notifications");
    case Feature::MouseLock: return QStringLiteral("pointer lock");
    case Feature::DesktopVideoCapture: return QStringLiteral("screen sharing");
    case Feature::DesktopAudioVideoCapture: return QStringLiteral("screen and audio sharing");
    default: return QString();
    }
#endif
}

int engineTypeForCapability(const QString &capability)
{
    const int index = capabilities().indexOf(capability);
    if (index < 0)
        return -1;
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    using Type = QWebEnginePermission::PermissionType;
    switch (index) {
    case 0: return int(Type::MediaVideoCapture);
    case 1: return int(Type::MediaAudioCapture);
    case 2: return int(Type::MediaAudioVideoCapture);
    case 3: return int(Type::Geolocation);
    case 4: return int(Type::Notifications);
    case 5: return int(Type::MouseLock);
    case 6: return int(Type::DesktopVideoCapture);
    case 7: return int(Type::DesktopAudioVideoCapture);
    case 8: return int(Type::ClipboardReadWrite);
    case 9: return int(Type::LocalFontsAccess);
    default: return -1;
    }
#else
    using Feature = QWebEnginePage::Feature;
    switch (index) {
    case 0: return int(Feature::MediaVideoCapture);
    case 1: return int(Feature::MediaAudioCapture);
    case 2: return int(Feature::MediaAudioVideoCapture);
    case 3: return int(Feature::Geolocation);
    case 4: return int(Feature::Notifications);
    case 5: return int(Feature::MouseLock);
    case 6: return int(Feature::DesktopVideoCapture);
    case 7: return int(Feature::DesktopAudioVideoCapture);
    default: return -1; // clipboard / local fonts did not exist before 6.7
    }
#endif
}

QString decisionFor(const QString &host, const QString &capability)
{
    QSettings settings;
    const QString key = settingsKey(host, capability);
    const QVariant stored = settings.value(key);
    if (stored.isValid())
        return stored.toString();

    // Fallback for profiles written before capabilities were named. The numeric
    // key can only be interpreted against this build's enum, which is exactly
    // why new decisions are no longer stored that way.
    const int legacyType = engineTypeForCapability(capability);
    if (legacyType >= 0) {
        const QVariant legacy = settings.value(QLatin1String(kDecisionPrefix) + host + '/'
                                               + QString::number(legacyType));
        if (legacy.isValid())
            return legacy.toString();
    }
    return QStringLiteral("ask");
}

void setDecision(const QString &host, const QString &capability, const QString &decision)
{
    if (host.isEmpty() || capability.isEmpty())
        return;
    QSettings settings;
    const QString key = settingsKey(host, capability);
    // "ask" means "forget this", not "remember the word ask": otherwise the
    // permissions page could never return a site to the prompting state, and
    // the row would look stored even though nothing was decided.
    if (decision == QLatin1String("ask"))
        settings.remove(key);
    else
        settings.setValue(key, decision);
}

QVariantMap allDecisions()
{
    QSettings settings;
    QVariantMap out; // host -> QVariantMap(capability -> decision)

    settings.beginGroup(QLatin1String(kDecisionPrefix));
    const QStringList hosts = settings.childGroups();
    settings.endGroup();

    for (const QString &host : hosts) {
        settings.beginGroup(QLatin1String(kDecisionPrefix) + host);
        const QStringList keys = settings.childKeys();
        QVariantMap row;
        for (const QString &key : keys) {
            // A numeric key is a pre-capability entry. Report it under the
            // capability name this build understands so the page shows a real
            // control; if this build cannot interpret the number, keep the raw
            // key so the entry stays visible rather than silently dropped.
            QString capability = key;
            bool numeric = false;
            const int type = key.toInt(&numeric);
            if (numeric) {
                const QString named = capabilityForEngineType(type);
                capability = named.isEmpty() ? key : named;
            }
            row.insert(capability, settings.value(key));
        }
        settings.endGroup();
        if (!row.isEmpty())
            out.insert(host, row);
    }
    return out;
}

void clearAll()
{
    QSettings settings;
    settings.remove(QLatin1String(kDecisionPrefix));
}

} // namespace PermissionStore
