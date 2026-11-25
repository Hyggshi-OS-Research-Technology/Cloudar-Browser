# File Protocol (`file://`) Security Considerations

This document outlines the security implications of opening local files via the `file://` protocol in Cloudar Browser.

## Overview

Cloudar Browser supports opening local files by typing absolute paths (e.g., `/home/user/file.html`) or `~` paths (e.g., `~/Downloads/file.html`) directly in the address bar. These paths are automatically converted to `file://` URLs.

## Security Notes

### Same-Origin Policy with Local Files

By default, Chromium (via QtWebEngine) blocks local files from loading additional files or scripts (e.g., AJAX calling other files). This prevents malicious local HTML files from accessing other system files.

- **Cloudar does NOT** set `--allow-file-access-from-files`, so same-origin restrictions remain active
- A local HTML file cannot use `fetch()` or `XMLHttpRequest` to read other local files

### XSS via Local HTML Files

If a local `.html` file contains malicious JavaScript, the scripts **can** execute within that `file://` context. Chromium does not block script execution in local files.

- **Risk:** If a user opens a malicious local HTML file (e.g., from a downloaded email attachment or USB drive), any embedded scripts will run
- **Mitigation:** Users should only open local files from trusted sources

### No Network Sandbox

When reading local files, the request does not go through the network stack's sandbox. However, this also means:

- Local file access is isolated from network-based attacks
- No data exfiltration via network requests from `file://` origins (blocked by same-origin policy)
- Generally **safer** than browsing the open web

## Directory listing (`cloudar://files/`)

Local folders can be browsed in a tab: **File → Open Folder…**, typing an
absolute path in the address bar, or the "Open Folder…" button on the listing
page. The listing is an ordinary internal page (`resources/files.html`) served
by `LightSchemeHandler`, backed by `LocalFileService`.

That is a new capability — a *web page* asking the browser to enumerate a
directory — so it is deliberately the narrowest version of it:

### The confinement rule

A directory can be listed **only if the user opened it** (or opened a folder
containing it, or a folder inside it). `LocalFileService::listDirectory()`
canonicalises the requested path and compares it against the set of roots
component-wise; anything else is refused with an `outside-root` error, which the
page renders as "This page can only list folders you opened."

What that rules out:

| Attempt | Result |
|:---|:---|
| `listDirectory('/etc')` from a page, nothing opened | refused — `outside-root` |
| `listDirectory('/tmp/x/user2')` when `/tmp/x/user` is the root | refused — component-wise comparison, not `startsWith()` |
| `listDirectory('/root/sub/../../etc')` | canonicalised first, then refused |
| A symlink inside the root pointing outside it | resolved by `canonicalFilePath()`, then refused |
| Walking `..` above the root from the page | the reply's `parent` is empty at the root, so the page cannot navigate up |
| Listing a *file* | refused — `not-a-directory`; the page gets names/sizes/dates, never contents |

Roots are **per session and never persisted**. Reopening the browser does not
leave a page able to list last week's directory.

### What a page can and cannot get

* **Can**: names, sizes, modification times, and a `file://` URL per entry.
* **Cannot**: file *contents*. Clicking a file navigates the tab to that
  `file://` URL, which is exactly what typing the path would have done, and the
  `file://` origin rules above then apply to it.
* **Cannot**: choose a folder for itself. The page's "Open Folder…" button asks
  the *window* to run the picker (`InternalBridge::requestFolderPicker`) — a page
  that could pick its own root would make the confinement meaningless.

### Why not just widen `LocalContentCanAccessFileUrls`?

Because that setting is a **profile-wide** switch. It would let any `cloudar://`
page (and any `file://` page) read arbitrary files through normal web APIs, in
every tab, forever. The listing route is per-root, per-session, read-only and
limited to metadata — a much smaller grant for the same feature.

`LocalContentCanAccessFileUrls` therefore stays **off** (see
`MainWindow`'s profile setup), and this feature does not touch it.

### Tests

`tests/tst_localfileservice.cpp` encodes the table above. It is a **security**
test, not a convenience test: run it with

```bash
cmake -B build -S . -DCLOUDAR_BUILD_TESTS=ON && cmake --build build --target tst_localfileservice
ctest --test-dir build --output-on-failure
```

CI runs it on every Linux job.

## Best Practices

1. Only open local files from trusted sources
2. Be cautious with downloaded `.html` files from unknown origins
3. Combined with "Absolute Security Mode" (which disables JavaScript), local file browsing becomes highly secure
4. The Force Download Directory (VD) feature ensures all downloaded files go to a designated location, reducing the risk of accidentally opening malicious files

## Related Settings

- **Absolute Security Mode** (Settings → Privacy & Security): Disables JavaScript and enforces HTTPS, adding protection when browsing
- **Force Download Directory (VD)** (Settings → Downloads): Forces all downloads to a specific directory for easier management and review