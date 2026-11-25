# Interface language

Cloudar Browser ships ten interface languages. The choice lives in
`QSettings` under `browser/language` and is applied to the browser's own chrome —
menus, toolbars, dialogs, the horizontal and vertical tab strips, the tab context
menu, the bookmarks bar, the downloads dialog, the permission prompt, the New Tab
page and every `cloudar://` page. Websites are untouched: their language comes
from the site and from `Accept-Language`, which this setting does not rewrite.

| Code | Language | Code | Language |
|:---|:---|:---|:---|
| `en` | English | `ru` | Русский |
| `vi` | Tiếng Việt | `pt` | Português |
| `ko` | 한국어 | `ja` | 日本語 |
| `fr` | Français | `zh` | 简体中文 |
| `de` | Deutsch | `es` | Español |

`system` is a value of its own: it follows `QLocale::system()`, and it is what an
untouched profile uses. It is not the same as `en` — on a Vietnamese desktop the
browser starts in Vietnamese without anyone choosing it.

## Where the setting lives

Settings → Appearance → **Language** (`cloudar://settings/`).

The card shows three measured facts rather than a claim:

* **Currently showing** — the language actually resolved, whether it came from the
  stored preference or from the OS locale.
* **Translation coverage** — `N of M interface strings` plus a percentage, read
  from the dictionary that is actually loaded. A language missing strings says so
  instead of pretending to be complete.
* **Where the dictionary came from** — `built in`, an external `lang/<code>.json`
  path, or nothing at all if it could not be loaded (in which case the UI falls
  back to English and the card says that).

Changing it applies **immediately**, with no restart: every open window listens to
`LanguageManager::languageChanged` and rebuilds its menus and labels.

## How a string is translated

Two routes, because the code has two kinds of call site.

### C++ — Qt's own machinery

`LanguageManager` installs a `DictionaryTranslator` (a `QTranslator`) on the
application. It is built from the **English dictionary**, so its keys are the
English source text, which means ordinary `tr()` works and needs no call-site
plumbing:

```cpp
auto *find = menu->addAction(tr("Find in Page"));
```

Accelerator markers survive: `tr("&File")` looks up `File` and re-attaches the
`&` in front of the translated word, so Alt+F still opens the File menu in
Vietnamese. A string that no dictionary contains makes `translate()` return an
empty `QString`, which makes Qt fall back to the English source — never a blank
label, never a raw key.

`get_text("key")` is still the right call for strings that are *data* rather than
literals: values read from JSON, table labels built in a loop, translation of
formatted sentences.

### `cloudar://` pages — `resources/i18n.js`

The internal pages are static HTML served from the binary; there is no build step
that could pre-render ten languages. `LightSchemeHandler` therefore serves the
**resolved** dictionary at `i18n.json` from every internal host (so each page can
fetch it same-origin), and `i18n.js` applies it:

```html
<h1 data-i18n="page_history">History</h1>
<input id="q" data-i18n-placeholder="history_search_placeholder" placeholder="Search history">
```

English stays in the markup as the pre-fetch and failure fallback. Script-built
text asks for the string:

```js
listEl.firstChild.textContent = CloudarI18n.t('history_empty', 'No history found.');
```

A page that renders rows from data can re-render once the dictionary lands by
defining `window.cloudarLanguageApplied(dict)` — `cloudar://permissions` does this
so its table headers follow the language.

## Adding a language

1. Copy `lang/en.json` to `lang/<code>.json` and translate the values. Keys never
   change.
2. Add `{"<code>", "<native name>"}` to `LanguageManager`'s `m_supportedLanguages`
   in `src/Language.cpp`.
3. Add `<file alias="lang/<code>.json">../lang/<code>.json</file>` to
   `resources/resources.qrc`.
4. `node tools/check-i18n.js`.

## Adding a string

Add the English text to `lang/en.json` under a new key, add the same key to every
other dictionary, then use it (`tr("…")` with that text, `get_text("key")`, or
`data-i18n="key"`). `tools/check-i18n.js` fails the build if a key is added to one
dictionary and not the others, if a `tr()`/`get_text()`/`data-i18n` reference has
no English entry, or if a newly added key is not referenced anywhere.

## What is not translated

* **Page content**, including page titles in tabs and the window title. The
  product name and the incognito marker around a title are translated; the title
  itself is the site's.
* **Settings page body copy.** The section names, page headings, card titles and
  the Language card are translated; the long explanatory paragraphs under each
  control are still English-only. They are the next piece of this work, not a
  claim that they are done.
* **Chromium's own UI strings** — the DevTools window, the PDF viewer, form
  validation bubbles. Those come from QtWebEngine's resource tree, which this
  project does not ship translations for.
* **`--help` output**, which is a plain C string printed before `QApplication`
  exists (see `src/main.cpp`: it has to be answerable without a display).

## Verification

`node tools/check-i18n.js` runs in CI before the Qt build and checks eight things,
each one a defect that was actually present at some point:

1. every language `LanguageManager` offers has a dictionary file;
2. no dictionary is missing a key another one has (and none has keys `en.json`
   does not, which would be unreachable);
3. no newly added English key is unreachable from `tr()`, `get_text()` or
   `data-i18n`;
4. every `tr("…")` in the C++ chrome has an English entry;
5. every `get_text("key")` has such a key;
6. every `data-i18n`/`data-i18n-placeholder` in an internal page has such a key;
7. every internal page loads `i18n.js`;
8. every dictionary is in `resources.qrc`, so an installed build can find it.

The gate's baseline (`tools/i18n-baseline.json`) is `lang/en.json` as it was when
the script was added, so the pre-existing reserved keys do not produce noise while
any key added later must be reachable.
