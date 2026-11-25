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
│   ├── background.js          wallpaper: browser-backed storage, media probes
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

The browser owns the wallpaper, not the page. A `cloudar://` page's
`localStorage` and IndexedDB belong to the renderer's storage partition, and this
build clears browsing data on exit unless the user opted into persistent
cookies — so a wallpaper kept there disappeared on restart. The durable copy is
native:

| Media | Where | Why |
|:---|:---|:---|
| colour / web URL | `QSettings` `browser/newTabWallpaper/{kind,value}` | a few dozen bytes |
| device image | `<AppDataLocation>/wallpaper/wallpaper.<ext>` | outside the storage partition, survives clearing browsing data |
| device video | `<AppDataLocation>/wallpaper/wallpaper.<ext>` | same slot, same code path |

One file per kind, so choosing ten wallpapers cannot leave a directory of
orphans. The suffix comes from the MIME the page reports with the bytes, and
`cloudar://newtab/media/wallpaper.<ext>` answers with the matching
Content-Type — a PNG is served as `image/png` instead of being mislabelled
`image/jpeg`.

The page caps an uploaded image at 2560 px on the long edge and re-encodes to
JPEG q=0.85 **only when it has to**; a file that is already small enough is
stored byte-for-byte in its original format, which is what keeps PNG
transparency and avoids compressing a screenshot twice on every restart.

Restore happens in two stages: the local copy synchronously (no flash of the
default background), then `initialState().wallpaper` once the QWebChannel reply
actually arrives. The second stage is the one that survives a restart.

Every `URL.createObjectURL()` goes through `setObjectUrl(slot, blob)`, which
revokes the previous handle in that slot. `releaseAll()` runs on `pagehide` and
when the page is hidden, so a background tab does not pin a decoded video in
memory for hours.

### Adjusting the frame

An image or a video can be panned and zoomed after it is chosen. The frame editor
opens by itself once a wallpaper has been applied, and again from **Adjust the
frame** in the Customize panel; drag to move, scroll or use the slider to zoom
(100%–400%), `Shift`-free arrow keys to nudge, `Esc` or **Done** to finish.

Three numbers describe it, stored in `QSettings` next to the wallpaper itself
(`browser/newTabWallpaper/{focusX,focusY,zoom}`):

| Value | Range | Meaning |
|:---|:---|:---|
| `focusX`, `focusY` | -1..1 | which point of the image is centred |
| `zoom` | 1..4 | 1 is the plain `object-fit: cover` fill |

`0, 0, 1` is the centred cover the page used before this feature, so every
wallpaper saved by an older build still renders identically.

The editor is **WYSIWYG on purpose**: it transforms the real wallpaper layer
rather than a preview box. A separate preview needs its own copy of the geometry
and its own idea of what `cover` means, which is how a panel ends up showing one
crop and applying another. Applying a wallpaper also announces
`cloudar:wallpaper-applied`, so nothing has to poll to notice.

Both layers are boxes with `object-fit: cover`, and the pan is computed in
**pixels from the layer's layout box**:

    overflow = (offsetWidth * zoom - innerWidth) / 2   (per axis)
    translate = focus * overflow

Percentages cannot express this: `cover` may crop only one axis (a 4:3 photo in a
16:9 window), and the layer box is not always the window size. The first version
of this used `focus * (zoom - 1) * 100%`, which is twice the real overflow —
dragging to the edge pulled the image past the window and exposed the background.
`tools/` carries no simulation of this; the invariant was verified offline over
3000 combinations of window size, image aspect ratio, zoom and focus: the image
always covers the window, an axis that exactly fits never moves, and focus ±1
lands the image edge exactly on the window edge.

The limits live in **two** places by necessity — `kMinZoom`/`kMaxZoom`/
`kMaxFocus` in `src/NewTabBridge.cpp` (which clamps, because a settings file can
be edited) and `MIN_ZOOM`/`MAX_ZOOM`/`MAX_FOCUS` in `scripts/background.js`
(which draws the slider). `tools/check-newtab.js` gate 6b fails the build if the
pairs disagree, so the slider can never offer a zoom the browser silently
changes.

### Which layer is showing

`scripts/background.js` has one answer to "what is the background right now":
`surface()` returns `'image'`, `'video'`, `'color'` or `'default'` from the
classes on `<body>`. Three places used to read the class list themselves — the
transform code, the frame editor's button, and the panel — and they drifted.

Every function that changes those classes calls `announceSurface()`, which fires
`cloudar:wallpaper-changed`. That exists because of a real bug: the **Adjust the
frame** button was greyed out over a wallpaper that was plainly visible. It asked
`isFramable()` at init and on `cloudar:wallpaper-applied` only, and a wallpaper
restored from IndexedDB arrives *asynchronously*, after init — so the button kept
a decision made before the wallpaper existed. Gate 7 now fails the build if a
function toggles `image-bg` / `video-bg` / `custom-bg` without announcing it.

### Remote wallpaper URLs

`cloudar://` is registered as a **secure** scheme, so Chromium treats it as a
secure origin. That has two consequences the panel has to handle out loud:

* `http://` media is blocked as mixed content, with no callback into the page.
  The panel refuses it up front and says why.
* `https://` media is only accepted after it has actually loaded. The page probes
  the URL with `Image`/`video` first, so a typo, a 404 or a blocked host produces
  an error message and leaves the current background untouched — instead of
  claiming success over an unchanged screen.
* The field takes a **file** address, not a page. An address with no media
  extension is rejected before any request is made, with a message naming what it
  actually is: `https://youtu.be/…` and other YouTube links get their own
  explanation. Playback from YouTube would need an embedded player — a
  `QWebEngine` view in the browser process — and `cloudar://` pages cannot embed
  one, so the honest answer is to say so rather than report "could not be
  loaded".

Storage failures are shown in the panel with the real `DOMException` name —
`QuotaExceededError` in particular is a normal outcome for video and the user
needs to know the wallpaper will not survive a reload.

## Adding a shortcut

Shortcuts live in `localStorage` (`cloudar.newtab.speeddial`); the page migrates
the previous `lightbrowser.newtab.speeddial` key once on first load. A shortcut
is page state, not browser state, which is why it is not in `QSettings` — but it
is sanitised on load (`sanitize()`), so a hand-edited or corrupted entry cannot
inject a `javascript:` URL into the tile.
