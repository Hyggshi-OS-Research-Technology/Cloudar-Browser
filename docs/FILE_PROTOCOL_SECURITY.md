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

## Best Practices

1. Only open local files from trusted sources
2. Be cautious with downloaded `.html` files from unknown origins
3. Combined with "Absolute Security Mode" (which disables JavaScript), local file browsing becomes highly secure
4. The Force Download Directory (VD) feature ensures all downloaded files go to a designated location, reducing the risk of accidentally opening malicious files

## Related Settings

- **Absolute Security Mode** (Settings → Privacy & Security): Disables JavaScript and enforces HTTPS, adding protection when browsing
- **Force Download Directory (VD)** (Settings → Downloads): Forces all downloads to a specific directory for easier management and review