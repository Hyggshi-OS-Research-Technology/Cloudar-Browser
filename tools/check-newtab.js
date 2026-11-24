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
