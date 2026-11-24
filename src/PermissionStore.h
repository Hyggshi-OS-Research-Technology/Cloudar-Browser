#pragma once

#include <QString>
#include <QVariantMap>

// ---------------------------------------------------------------------------
// Where the browser remembers "this site may use the camera".
//
// The engine hands us a numeric permission type, and the two Qt versions this
// project supports do not agree on the numbers: Qt < 6.7 uses
// QWebEnginePage::Feature, Qt >= 6.7 uses QWebEnginePermission::PermissionType.
// Storing the number meant a preference saved by a Qt 6.2 build was read back
// as a *different* capability by a Qt 6.8 build — silently, and in the
// permissive direction if the numbers happened to line up badly. Keys are
// therefore capability names ("camera", "microphone", ...), which survive a Qt
// upgrade; the numeric form is still read as a fallback so a profile written by
// an older build keeps its decisions.
//
// Both MainWindow (which answers the live prompt) and the cloudar://permissions
// page go through this so the two can never disagree about what "allow" means.
// ---------------------------------------------------------------------------
namespace PermissionStore {

// "camera", "microphone", "location", "notifications", ... for a type from
// whichever enum the current Qt version uses, or an empty string if unknown.
QString capabilityForEngineType(int engineType);

// The reverse. Returns -1 when the name has no engine equivalent, which is what
// the settings page uses for the capabilities the engine cannot grant.
int engineTypeForCapability(const QString &capability);

// "ask" (no decision stored), "allow" or "deny".
QString decisionFor(const QString &host, const QString &capability);
void setDecision(const QString &host, const QString &capability, const QString &decision);

// host -> { capability: decision }, for every decision stored. Capabilities the
// engine does not know are reported under their raw key so a stale entry is
// visible instead of invisible.
QVariantMap allDecisions();

void clearAll();

} // namespace PermissionStore
