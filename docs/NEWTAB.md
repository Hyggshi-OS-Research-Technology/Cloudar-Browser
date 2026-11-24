# New Tab page (`cloudar://newtab`)

The New Tab page is a small application of its own, not a blob of markup inside
the browser. It is split into files, served from the resource system, and talks
to the browser through one QWebChannel object.

## Layout

```
resources/newtab/
├── index.html                 markup only (no logic)
├── styles/
│   ├── base.css               tokens, reset, page shell, motion rules
│   ├── themes.css             the classic (Chrome 2009) chrome
│   ├── background.css         wallpaper surface + media controls
│   └── components.css         search bar, tiles, speed dial, panel, dialog
├── scripts/
│   ├── app.js                 bridge plumbing, chrome style, helpers
│   ├── search.js              search submit + URL-vs-query rules
│   ├── speed-dial.js          shortcut state, storage, URL normalisation
│   ├── background.js          wallpaper: IndexedDB, object-URL lifecycle
│   └── customize.js           the panel's controls
└── components/
    ├── search-bar.js          engine label (a fact from the bridge)
    ├── quick-access.js        speed-dial DOM + real favicons
    └── customize-panel.js     recent-history strip
```

`cloudar://newtab/assets/<path>` maps 1:1 onto these files
(`LightSchemeHandler::serveNewTabAsset`), so an inspector request or a stack
trace names `styles/base.css` rather than line 940 of an HTML file.

`tools/check-newtab.js` (run in CI before the Qt build) fails the build when:

- `index.html` references an asset that is missing or absent from
  `resources.qrc`,
- a file under `resources/newtab/` is never loaded,
- the page calls `bridge.<name>()` and `NewTabBridge` has no such slot,
- the classic class is applied but no `html.classic` rule exists — the exact
  defect this page shipped with,
- any script fails to parse.

## The two chromes

`styles/themes.css` implements the classic chrome for this page: Tahoma 12 px,
`#e8e8e8` window, white panels with 1 px `#a0a0a0` borders, no radii, no
shadows, no drifting gradient, no floating blobs. This matches the palette
`settings.html`, `history.html` and the other manager pages already use.

The style is decided twice, on purpose:

1. an inline script in `<head>` reads `cloudar.chromeStyle` from localStorage
   before the stylesheets paint — the native side writes that key on every
   internal page load (`BrowserTabWidget::wireView`), so there is no dark flash
   inside a beige window;
2. `scripts/app.js` then asks `newTabBridge.initialState().chromeStyle`, which
   reports the style in force for **this run**. That is the only way
   `--classic` / `CLOUDAR_THEME` can reach this page, because those deliberately
   do not rewrite the saved preference.

## Internal API (`newTabBridge`)

Registered in `MainWindow` next to `native` and `internalBridge`, on the channel
that `BrowserTabWidget` attaches only to `cloudar://` pages, so no website can
reach it.

| Call | Returns |
|:---|:---|
| `initialState()` | `{ chromeStyle, searchTemplate, searchName, version, bookmarks[], recent[] }` |
| `faviconUrl(pageUrl)` | `cloudar://newtab/favicon?u=…` when cached, `""` when not (and starts a fetch) |
| `openUrl(url)` | nothing; navigation happens through `MainWindow`, so an internal page focuses its existing tab |

`faviconReady(pageUrl, iconUrl)` is emitted when a background fetch stores an
icon, so a tile updates in place instead of the page polling.

## Real favicons

`FaviconStore` receives every icon Chromium decodes (`iconChanged` →
`BrowserTabWidget::wireView`), keeps the largest pixmap as a PNG in
`<browser data>/favicons/<host>.png`, and serves it back through the scheme
handler. Resolution order on a tile:

1. the icon the browser already decoded for that host,
2. `https://<host>/favicon.ico`, fetched once and cached,
3. a coloured square with the site's first letter.

Step 3 is a fallback. It used to be the whole design.

## Wallpaper storage

| Media | Where | Why |
|:---|:---|:---|
| colour / web URL | `localStorage` (`cloudar.newtab.background`) | a few dozen bytes |
| device image | IndexedDB `cloudar_wallpaper_db/images` | a 1920×1080 JPEG is 300–800 KB as base64 |
| device video | IndexedDB `cloudar_wallpaper_db/videos` | same store, same code path |

The image cap is 2560 px on the long edge, re-encoded at JPEG q=0.85 — and the
original bytes are kept untouched when the file is already small, so restarting
never re-compresses a screenshot twice.

Every `URL.createObjectURL()` goes through `setObjectUrl(slot, blob)`, which
revokes the previous handle in that slot. `releaseAll()` runs on `pagehide` and
when the page is hidden, so a background tab does not pin a decoded video in
memory for hours.

Storage failures are shown in the panel with the real `DOMException` name —
`QuotaExceededError` in particular is a normal outcome for video and the user
needs to know the wallpaper will not survive a reload.

## Adding a shortcut

Shortcuts live in `localStorage` (`cloudar.newtab.speeddial`); the page migrates
the previous `lightbrowser.newtab.speeddial` key once on first load. A shortcut
is page state, not browser state, which is why it is not in `QSettings` — but it
is sanitised on load (`sanitize()`), so a hand-edited or corrupted entry cannot
inject a `javascript:` URL into the tile.
