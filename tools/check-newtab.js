#!/usr/bin/env node
/*
 * Static checks for the New Tab page, run in CI before the (much slower) Qt
 * build. Everything here is a class of mistake that has actually shipped in this
 * repository before, so each check names the failure it prevents:
 *
 *   1. a stylesheet or script referenced by index.html that is not in the qrc
 *      -> the page loads with no CSS and looks broken, but nothing errors loudly
 *   2. a file in resources/newtab/ that nothing references
 *      -> dead code that slowly rots
 *   3. a `bridge.<name>` call in the page that NewTabBridge does not declare
 *      -> the empty-shell bug that all five manager pages had
 *   4. `html.classic` referenced but no rule defined
 *      -> the exact defect reported for the New Tab page: the class was toggled
 *         and no CSS answered it
 *
 * Usage: node tools/check-newtab.js
 */
'use strict';

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const NT = path.join(ROOT, 'resources', 'newtab');
const QRC = path.join(ROOT, 'resources', 'resources.qrc');
const BRIDGE_CPP = path.join(ROOT, 'src', 'NewTabBridge.cpp');
const BRIDGE_H = path.join(ROOT, 'src', 'NewTabBridge.h');

const errors = [];
const notes = [];

function read(p) {
  return fs.readFileSync(p, 'utf8');
}

function walk(dir, out = []) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) walk(full, out);
    else out.push(full);
  }
  return out;
}

// --- 1 & 2: every asset referenced, every file used -------------------------
const html = read(path.join(NT, 'index.html'));
const refs = new Set();
for (const m of html.matchAll(/(?:href|src)="assets\/([^"]+)"/g)) refs.add(m[1]);

const qrc = read(QRC);
for (const ref of refs) {
  const rel = 'newtab/' + ref;
  if (!qrc.includes(`<file>${rel}</file>`)) {
    errors.push(`index.html references assets/${ref} but resources.qrc has no ${rel}`);
  }
  if (!fs.existsSync(path.join(NT, ref))) {
    errors.push(`index.html references assets/${ref}, which does not exist`);
  }
}

for (const file of walk(NT)) {
  const rel = path.relative(NT, file).split(path.sep).join('/');
  if (rel === 'index.html') continue;
  if (rel.startsWith('scripts/') || rel.startsWith('components/') ||
      rel.startsWith('styles/')) {
    if (!refs.has(rel)) errors.push(`${rel} exists but nothing loads it`);
  }
  if (!qrc.includes(`<file>newtab/${rel}</file>`)) {
    errors.push(`${rel} is not listed in resources.qrc, so it will not be bundled`);
  }
}

// --- 3: bridge surface ------------------------------------------------------
// Every `newTabBridge`/`bridge.x` call the page makes must exist as a slot in
// NewTabBridge.h. This is the check that would have caught the five internal
// pages calling a QWebChannel object that was never registered.
const bridgeH = read(BRIDGE_H);
const declared = new Set(
  [...bridgeH.matchAll(/^\s{4}(?:[\w:<>\s]+?)\s(\w+)\s*\(/gm)].map((m) => m[1])
);

const jsFiles = [...refs]
  .filter((r) => r.endsWith('.js'))
  .map((r) => path.join(NT, r));

const called = new Set();
for (const file of jsFiles) {
  const src = read(file);
  for (const m of src.matchAll(/\bbridge\.(\w+)\s*\(/g)) called.add(m[1]);
  for (const m of src.matchAll(/\bnewTabBridge\.(\w+)\s*\(/g)) called.add(m[1]);
}

for (const name of called) {
  if (name === 'connect') continue; // QWebChannel signal plumbing
  if (!declared.has(name)) {
    errors.push(`page calls bridge.${name}() but NewTabBridge has no such slot`);
  }
}

// Objects the page expects on the channel must be registered somewhere.
const mainWindow = read(path.join(ROOT, 'src', 'MainWindow.cpp'));
for (const obj of ['newTabBridge']) {
  const hit = mainWindow.includes(`registerObject("${obj}"`);
  if (!hit) errors.push(`MainWindow never registers the "${obj}" channel object`);
  if (!mainWindow.includes(`WebChannel`)) notes.push('no channel found in MainWindow');
}

// --- 4: the classic style must actually be defined --------------------------
const css = [...refs]
  .filter((r) => r.endsWith('.css'))
  .map((r) => read(path.join(NT, r)))
  .join('\n');
if (!/html\.classic\b/.test(css)) {
  errors.push('no html.classic rules: the classic chrome would leave this page dark');
}
if (!html.includes("classList.add('classic')")) {
  errors.push('index.html never applies the classic class before first paint');
}
// The bridge must be able to report the style for this run (--classic must not
// be overridden by whatever localStorage happened to hold).
const bridgeCpp = read(BRIDGE_CPP);
if (!/chromeStyle/.test(bridgeCpp)) {
  errors.push('NewTabBridge never reports chromeStyle, so --classic cannot reach the page');
}

// --- 5: no synchronous use of a value-returning bridge slot ---------------
// qwebchannel.js delivers a slot's return value to a trailing callback and
// returns `undefined` itself. So `var x = bridge.foo()` is *always* undefined,
// which looks exactly like "the native side returned nothing". That mistake is
// what made the saved wallpaper and the real favicons both appear broken.
//
// Only slots that actually return something matter here; void slots such as
// openUrl() are fire-and-forget by design and must NOT take a callback.
const VALUE_SLOTS = new Set(['initialState', 'faviconUrl', 'saveWallpaperMedia', 'wallpaperState', 'wallpaperMediaUrl']);
// Strip line comments so prose about the API does not trip the check.
const stripComments = (src) =>
  src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/(^|[^:\\])\/\/[^\n]*/g, '$1');

for (const file of jsFiles) {
  const src = stripComments(read(file));
  // `!bridge.foo` is an existence guard, not a call.
  const callPattern = /(?<![!.\w])(?:bridge|newTabBridge|api)\.(\w+)\s*\(/g;
  for (const m of src.matchAll(callPattern)) {
    if (!VALUE_SLOTS.has(m[1])) continue;

    // Find the matching close paren, skipping over string literals so a ")"
    // inside a message cannot end the argument list early.
    let i = (m.index || 0) + m[0].length;
    let depth = 1;
    let start = i;
    let quote = null;
    while (i < src.length && depth > 0) {
      const ch = src[i];
      if (quote) {
        if (ch === '\\') { i += 2; continue; }
        if (ch === quote) quote = null;
      } else if (ch === '"' || ch === "'" || ch === '`') {
        quote = ch;
      } else if (ch === '(') {
        depth++;
      } else if (ch === ')') {
        depth--;
        if (depth === 0) break;
      }
      i++;
    }
    const args = src.slice(start, i);
    // Split on commas at nesting depth 0 only: a comma inside the callback's own
    // body is not an argument separator, which is what made the first version of
    // this check report a correct call as broken.
    let lastArg = '';
    let pdepth = 0;
    let pq = null;
    const parts = [];
    for (let k = 0; k < args.length; k++) {
      const ch = args[k];
      if (pq) {
        if (ch === '\\') { lastArg += ch + (args[++k] || ''); continue; }
        if (ch === pq) pq = null;
        lastArg += ch;
        continue;
      }
      if (ch === '"' || ch === "'" || ch === '`') { pq = ch; lastArg += ch; continue; }
      if ('([{'.includes(ch)) pdepth++;
      if (')]}'.includes(ch)) pdepth--;
      if (ch === ',' && pdepth === 0) { parts.push(lastArg); lastArg = ''; continue; }
      lastArg += ch;
    }
    lastArg = lastArg.trim();
    const isFn = /^(?:function\b|\()/.test(lastArg) || /=>/.test(lastArg);
    if (!isFn) {
      errors.push(
        `${path.relative(ROOT, file)}: ${m[1]}() is called without a callback — ` +
        `qwebchannel returns undefined, so the value would be silently lost`
      );
    }
  }
}

// --- 6: reading a property off something that is not in scope ---------------
// The specific defect this prevents, reported from a real run:
//
//     reader.onload = function () {          // no parameter
//       ...
//       img.src = e.target.result;           // ReferenceError: e is not defined
//     };
//
// It threw inside an async FileReader callback, so nothing was logged to the
// page except an uncaught error, img.src was never assigned, the image's own
// load event never fired, and a local wallpaper did nothing at all while the
// panel showed no message. Silent, and invisible to every other check here.
//
// Only `<ident>.target` is checked: that is the event-object property this bug
// class always reaches for, and it is unambiguous — a name used as `.target`
// must be a parameter of an enclosing function or declared in the file.
const KNOWN_GLOBALS = new Set([
  'window', 'document', 'console', 'navigator', 'location', 'localStorage',
  'indexedDB', 'URL', 'Math', 'JSON', 'Object', 'Array', 'String', 'Number',
  'Date', 'Error', 'Promise', 'Image', 'FileReader', 'Blob', 'self', 'this',
  'globalThis', 'HTMLElement', 'Event', 'performance',
]);

for (const file of jsFiles) {
  const src = stripComments(read(file));
  const declaredInFile = new Set(
    [...src.matchAll(/\b(?:var|let|const|function)\s+([A-Za-z_$][\w$]*)/g)].map((m) => m[1])
  );

  // Walk forward, keeping a stack of function scopes so `.target` can be resolved
  // against the parameter lists that actually enclose it.
  const scopes = [];
  let depth = 0;
  let i = 0;
  let pendingParams = null;
  while (i < src.length) {
    const ch = src[i];

    if (ch === '"' || ch === "'" || ch === '`') {
      const quote = ch;
      i++;
      while (i < src.length) {
        if (src[i] === '\\') { i += 2; continue; }
        if (src[i] === quote) { i++; break; }
        i++;
      }
      continue;
    }

    if (src.startsWith('function', i) && !/[\w$]/.test(src[i - 1] || '')) {
      const open = src.indexOf('(', i);
      const close = open < 0 ? -1 : (() => {
        let d = 0;
        for (let k = open; k < src.length; k++) {
          if (src[k] === '(') d++;
          else if (src[k] === ')') { d--; if (d === 0) return k; }
        }
        return -1;
      })();
      if (close < 0) { i++; continue; }
      pendingParams = src.slice(open + 1, close)
        .split(',')
        .map((t) => t.trim().split('=')[0].trim())
        .filter(Boolean);
      i = close + 1;
      continue;
    }

    // Arrow functions: `(a, b) => {` and `a => {`
    if (ch === '=' && src[i + 1] === '>' && !pendingParams) {
      let params = [];
      let k = i - 1;
      while (k >= 0 && /\s/.test(src[k])) k--;
      if (src[k] === ')') {
        let d = 0;
        const closeK = k;
        for (; k >= 0; k--) {
          if (src[k] === ')') d++;
          else if (src[k] === '(') { d--; if (d === 0) break; }
        }
        params = src.slice(k + 1, closeK).split(',')
          .map((t) => t.trim().split('=')[0].trim()).filter(Boolean);
      } else {
        let m = '';
        for (; k >= 0 && /[\w$]/.test(src[k]); k--) m = src[k] + m;
        if (m) params = [m];
      }
      pendingParams = params;
      i += 2;
      continue;
    }

    if (ch === '{') {
      depth++;
      scopes.push({ depth, params: pendingParams || [] });
      pendingParams = null;
      i++;
      continue;
    }
    if (ch === '}') {
      while (scopes.length && scopes[scopes.length - 1].depth === depth) scopes.pop();
      depth--;
      i++;
      continue;
    }

    if (/[A-Za-z_$]/.test(ch)) {
      let m = '';
      let k = i;
      for (; k < src.length && /[\w$]/.test(src[k]); k++) m += src[k];
      // `<ident>.target`
      if (src[k] === '.' && src.startsWith('.target', k) &&
          !/[\w$]/.test(src[k + 7] || '')) {
        const inScope = scopes.some((sc) => sc.params.includes(m));
        if (!inScope && !declaredInFile.has(m) && !KNOWN_GLOBALS.has(m)) {
          errors.push(
            `${path.relative(ROOT, file)}: reads .target off "${m}", which is not a ` +
            `parameter of any enclosing function and is not declared — this is a ` +
            `ReferenceError inside a callback, so the code after it silently never runs`
          );
        }
      }
      i = k;
      continue;
    }

    if (!/\s/.test(ch)) pendingParams = null;
    i++;
  }
}

// --- 6b: the frame editor contract -----------------------------------------
// Two ways "adjust the frame" can rot without any error being logged:
//
//   a. the page and the browser disagree about the limits, so the slider offers a
//      zoom the bridge silently clamps — the slider says 500% and the wallpaper
//      stops changing;
//   b. a component calls CloudarBackground.<fn>() that the module does not
//      return, which is a button that throws when clicked.
//
// Both are checked against the code, not against a list kept in this file.
const BG_JS = path.join(NT, 'scripts', 'background.js');
const bgJs = read(BG_JS);

// The values the C++ side enforces...
const cppLimits = {};
for (const m of read(BRIDGE_CPP).matchAll(
    /constexpr double k(Min|Max)Zoom = ([\d.]+);|constexpr double kMaxFocus = ([\d.]+);/g)) {
  if (m[1]) cppLimits[m[1].toLowerCase() === 'min' ? 'min' : 'max'] = Number(m[2]);
  else cppLimits.focus = Number(m[3]);
}
// ...and the values the page draws with.
const jsLimits = {};
for (const m of bgJs.matchAll(/var (MIN_ZOOM|MAX_ZOOM|MAX_FOCUS) = ([\d.]+);/g)) {
  jsLimits[m[1]] = Number(m[2]);
}
const limitPairs = [
  ['zoom min', cppLimits.min, jsLimits.MIN_ZOOM, 'kMinZoom', 'MIN_ZOOM'],
  ['zoom max', cppLimits.max, jsLimits.MAX_ZOOM, 'kMaxZoom', 'MAX_ZOOM'],
  ['focus', cppLimits.focus, jsLimits.MAX_FOCUS, 'kMaxFocus', 'MAX_FOCUS'],
];
for (const [what, a, b, aName, bName] of limitPairs) {
  if (a === undefined) errors.push(`NewTabBridge.cpp no longer defines ${aName}`);
  if (b === undefined) errors.push(`scripts/background.js no longer defines ${bName}`);
  if (a !== undefined && b !== undefined && a !== b) {
    errors.push(`${what} limit disagrees: ${aName}=${a} in C++ but ${bName}=${b} in JS — ` +
                `the editor would offer a value the browser silently clamps`);
  }
}

// Public surface of the background module: the keys of its return { ... }.
const retIndex = bgJs.lastIndexOf('  return {');
if (retIndex < 0) {
  errors.push('scripts/background.js has no `return {` block to read the public API from');
} else {
  const exported = new Set(
    [...bgJs.slice(retIndex).matchAll(/^\s{2,4}(\w+):/gm)].map((m) => m[1])
  );
  for (const file of jsFiles) {
    const src = stripComments(read(file));
    for (const m of src.matchAll(/\bCloudarBackground\.(\w+)\s*\(/g)) {
      if (!exported.has(m[1])) {
        errors.push(`${path.relative(ROOT, file)} calls CloudarBackground.${m[1]}() ` +
                    `but background.js does not export it`);
      }
    }
  }
}

// --- 7: wallpaper surface signals + theme tokens ----------------------------
// Two failures that only show up at runtime on a real machine:
//
//   a. a path changes which wallpaper layer is visible (image-bg / video-bg /
//      custom-bg) but does not announce it, so the "Adjust the frame" button
//      keeps whatever it decided at startup. That is the bug a screenshot caught:
//      the button greyed out over a wallpaper restored from IndexedDB, because a
//      restore arrives *after* the editor initialised and nothing re-asked.
//   b. a CSS custom property is used but defined in only one theme, so half the
//      UI is unstyled in the other. This is the class of defect the classic
//      chrome review was about.
const BG_SRC = stripComments(read(BG_JS));
// Group the toggles by function: every function that changes the surface must
// also announce it. An earlier version counted calls globally, which passed a
// function that removed video-bg silently while a sibling announced twice.
function functionsOf(src) {
  const out = [];
  let i = 0;
  const re = /function\s+([\w$]+)\s*\([^)]*\)\s*{/g;
  let m;
  while ((m = re.exec(src))) {
    const start = m.index;
    let depth = 0;
    let k = re.lastIndex - 1;
    for (; k < src.length; k++) {
      if (src[k] === '{') depth++;
      else if (src[k] === '}') { depth--; if (depth === 0) break; }
    }
    out.push({ name: m[1], body: src.slice(start, k + 1) });
    i++;
  }
  return out;
}
for (const fn of functionsOf(BG_SRC)) {
  const toggles = [...fn.body.matchAll(/classList\.(?:add|remove|toggle)\(\s*'(image-bg|video-bg|custom-bg)'/g)];
  if (!toggles.length) continue;
  if (!/announceSurface\(\)/.test(fn.body)) {
    errors.push(`${fn.name}() in scripts/background.js changes the wallpaper surface ` +
                `(${[...new Set(toggles.map((t) => t[1]))].join(', ')}) but never calls ` +
                `announceSurface() — controls that read the surface would not be told`);
  }
}

// Every class the CSS keys the background off must be toggled somewhere.
for (const cls of ['image-bg', 'video-bg', 'custom-bg']) {
  if (!new RegExp(`classList\\.(?:add|toggle)\\(\\s*'${cls}'`).test(BG_SRC)) {
    errors.push(`background.css styles body.${cls} but no code ever adds that class`);
  }
}

// Custom properties: defined in every theme block that redefines the palette.
const allCss = {};
for (const file of walk(NT).filter((f) => f.endsWith('.css'))) {
  allCss[path.relative(ROOT, file)] = read(file);
}
const baseCss = allCss['resources/newtab/styles/base.css'] || '';
const baseTokens = new Set(
  [...baseCss.matchAll(/^\s{2}(--[\w-]+):/gm)].map((m) => m[1])
);
// A token is "defined" if it appears as a declaration anywhere: palette tokens
// live in :root / html.classic, but a component may scope one to itself
// (--panel-width on .panel) and that is legitimate.
const declaredTokens = new Set(
  [...Object.values(allCss).join('\n').matchAll(/(?:^|[\s{;])(--[\w-]+)\s*:/gm)].map((m) => m[1])
);
const usedTokens = new Set();
for (const [file, src] of Object.entries(allCss)) {
  if (file.endsWith('base.css')) continue;
  for (const m of src.matchAll(/var\((--[\w-]+)/g)) usedTokens.add(m[1]);
}
const themeSrc = allCss['resources/newtab/styles/themes.css'] || '';
const classicTokens = new Set(
  [...themeSrc.matchAll(/^\s{2}(--[\w-]+):/gm)].map((m) => m[1])
);

// Every alternate palette, not just the classic one. The New Tab page now has
// three surfaces for the modern chrome (dark default, pinned light,
// system-following light) plus the classic one, and a *surface* token missing
// from one of them leaves that window half-styled — the defect the classic
// review was about, so the check has to cover all of them.
//
// Only surface tokens are required, and that is deliberate: the light palette
// keeps the same accent, radii and fonts as the dark one, and demanding
// redefinitions for those would force noise into every palette and teach people
// to ignore this check. What must never be inherited across palettes is a
// colour that something paints with.
const SURFACE_TOKEN = /^--(bg|panel|tile|field|text|muted|shadow|scrim)/;

function paletteBlocks(src) {
  // Comments first: the prose above explains `html.classic` and carries braces,
  // which a naive selector scan happily reads as a rule.
  const clean = src.replace(/\/\*[\s\S]*?\*\//g, '');
  const blocks = [];
  // Selectors can carry :not(...) as well: the light palette is
  // html[data-color-mode="light"]:not(.classic), and a pattern that stopped at
  // the attribute silently found only the classic block.
  const re = /(html(?:\[[^\]]*\])*(?::not\([^)]*\))*(?:\.classic)?)\s*\{([^}]*)\}/g;
  let m;
  while ((m = re.exec(clean))) {
    const decls = [...m[2].matchAll(/(--[\w-]+)\s*:/g)].map((x) => x[1]);
    if (decls.length) blocks.push({ selector: m[1].trim(), tokens: new Set(decls) });
  }
  return blocks;
}
const palettes = paletteBlocks(themeSrc);

for (const token of usedTokens) {
  if (!declaredTokens.has(token)) {
    errors.push(`${token} is used in CSS but never declared — it would resolve to nothing`);
  }
  // A surface token must be redefined in *every* alternate palette, or that
  // surface inherits whatever the previous one left behind.
  if (!baseTokens.has(token) || !SURFACE_TOKEN.test(token)) continue;
  for (const block of palettes) {
    if (!block.tokens.has(token)) {
      errors.push(`${token} is a surface token in base.css but ${block.selector} ` +
                  `never redefines it — that surface would inherit the wrong value`);
    }
  }
}
if (palettes.length < 3) {
  errors.push(`only ${palettes.length} palette block(s) found in themes.css — the ` +
              `light/dark/classic check would silently pass on an empty set`);
}
// The specific regression: the panel needs an opaque surface so a photo cannot
// be read through its text.
for (const token of ['--panel-solid']) {
  if (!baseTokens.has(token)) errors.push(`${token} is not defined in base.css`);
  if (!classicTokens.has(token)) errors.push(`${token} is not defined for html.classic in themes.css`);
}

// --- 8: every internal page answers light and dark --------------------------
// A browser that can be light has to be light everywhere. A dark-only internal
// page inside a light window is the half-applied style this project treats as a
// defect — <html> would be light while the page painted itself dark.
//
// Each page needs both halves: a light palette, and the bootstrap that reads the
// cached mode before first paint (otherwise it flashes dark on every load).
const INTERNAL_PAGES = ['about', 'bookmarks', 'downloads', 'extensions', 'history',
                        'permissions', 'settings'];
for (const page of INTERNAL_PAGES) {
  const file = path.join(ROOT, 'resources', `${page}.html`);
  if (!fs.existsSync(file)) {
    errors.push(`resources/${page}.html is listed as an internal page but does not exist`);
    continue;
  }
  const src = read(file);
  if (!/data-color-mode="light"/.test(src)) {
    errors.push(`resources/${page}.html has no light palette — a light window would ` +
                `show this page dark`);
  }
  if (!/prefers-color-scheme:\s*light/.test(src)) {
    errors.push(`resources/${page}.html does not handle prefers-color-scheme: light, ` +
                `so System mode cannot follow the OS before the bridge answers`);
  }
  if (!/cloudar\.colorMode/.test(src)) {
    errors.push(`resources/${page}.html never reads cloudar.colorMode, so it would ` +
                `flash the dark palette before the native side's value arrives`);
  }
}

// --- 6: syntax --------------------------------------------------------------
for (const file of jsFiles) {
  const src = read(file);
  try {
    // eslint-disable-next-line no-new-func
    new Function(src);
  } catch (e) {
    errors.push(`${path.relative(ROOT, file)}: syntax error: ${e.message}`);
  }
}

if (errors.length) {
  for (const e of errors) console.error(`::error title=newtab::${e}`);
  console.error(`\ncheck-newtab: ${errors.length} problem(s)`);
  process.exit(1);
}
console.log(`check-newtab: ok (${refs.size} assets, ${called.size} bridge calls)`);
