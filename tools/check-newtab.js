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

// --- 5: syntax --------------------------------------------------------------
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
