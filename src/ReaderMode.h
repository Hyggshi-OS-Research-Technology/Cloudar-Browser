#pragma once

#include <QString>

// Reader mode works by injecting a self-contained JS toggle into the
// current page (same mechanism as ExtensionManager's userscripts) rather
// than needing anything on the C++ side to track per-tab state — the
// script itself checks for its own overlay element and adds/removes it,
// so re-running the same script is what makes the toolbar button a toggle.
namespace ReaderMode {

// Returns the JS to inject. Evaluates to the string "opened", "closed", or
// "no-article" (couldn't find enough article-like content) — callers can
// use this to surface a status message.
QString toggleScript();

} // namespace ReaderMode
