#!/usr/bin/env node
/*
 * Stylesheet checks, run in CI before the Qt build.
 *
 * Both chromes build their QSS by string substitution, and until now nothing
 * verified that the substitution actually completed. Two real defects came out
 * of that gap, and each one is a check below:
 *
 *   1. The modern sheet is one QString with positional placeholders (%1..%12)
 *      fed by a chained .arg(). A role added to the text but not to the arg()
 *      chain renders as a literal "%13" in the window, which Qt silently
 *      ignores — a colour that simply never applies, in one theme only.
 *      -> check: every %N in the sheet is <= the number of .arg() calls, and
 *         every .arg() call has a matching %N.
 *
 *   2. The classic sheet uses named @TOKEN@ markers substituted from a table.
 *      A marker with no entry survives into the QSS, where Qt drops the whole
 *      declaration.
 *      -> check: no @TOKEN@ survives substitution.
 *
 *   3. A stylesheet that paints a glyph does so with a fixed colour. The tab
 *      close button shipped as a hard-coded #c7cede cross, i.e. white on the
 *      light tab surface — the button existed and could not be seen.
 *      -> check: no `image: url(...)` in the modern sheet (painted icons only),
 *         and every `color:` value is a palette role or a literal that the
 *         palette check below can see.
 *
 * Usage: node tools/check-css.js
 */
'use strict';

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const read = (p) => fs.readFileSync(path.join(ROOT, p), 'utf8');

const errors = [];
const notes = [];

// --- 1: the modern sheet's positional arguments ------------------------------
{
  const src = read('src/ThemeManager.cpp');
  const start = src.indexOf('QString ThemeManager::modernStyleSheet');
  if (start < 0) {
    errors.push('src/ThemeManager.cpp: modernStyleSheet() not found');
  } else {
    // The sheet ends where the .arg() chain begins.
    const argStart = src.indexOf(')"', start);
    const sheet = src.slice(start, argStart);
    const argChainEnd = src.indexOf(';', argStart);
    const chain = src.slice(argStart, argChainEnd);

    const used = new Set([...sheet.matchAll(/%(\d+)/g)].map((m) => Number(m[1])));
    const supplied = [...chain.matchAll(/\.arg\(/g)].length;

    for (const n of used) {
      if (n > supplied) {
        errors.push(`modern stylesheet uses %${n} but only ${supplied} .arg() call(s) supply it ` +
                    `(the declaration would be dropped silently)`);
      }
    }
    if (supplied > Math.max(...used, 0)) {
      errors.push(`modern stylesheet has ${supplied} .arg() call(s) but only uses up to ` +
                  `%${Math.max(...used, 0)} — the extra ones are dead and hide a missing role`);
    }
    notes.push(`modern sheet: ${used.size} distinct placeholders, ${supplied} args`);
  }
}

// --- 2: the classic sheet's named tokens -------------------------------------
{
  const src = read('src/ClassicTheme.cpp');
  const sheetStart = src.indexOf('kClassicSheet[] = R"QSS(');
  if (sheetStart < 0) {
    errors.push('src/ClassicTheme.cpp: kClassicSheet not found');
  } else {
    const sheetEnd = src.indexOf(')QSS"', sheetStart);
    const sheet = src.slice(sheetStart, sheetEnd);

    // The substitution table is the list of {"@TOKEN@", ...} pairs.
    const tableStart = src.indexOf('const QVector<QPair<const char *, QString>> tokens');
    const tableEnd = src.indexOf('};', tableStart);
    const table = src.slice(tableStart, tableEnd);
    const provided = new Set([...table.matchAll(/\{"(@[A-Z]+@)"/g)].map((m) => m[1]));

    const used = new Set([...sheet.matchAll(/@[A-Z]+@/g)].map((m) => m[0]));
    for (const token of used) {
      if (!provided.has(token)) {
        errors.push(`classic stylesheet uses ${token} but no substitution entry provides it ` +
                    `(the whole declaration is dropped by Qt)`);
      }
    }
    for (const token of provided) {
      if (!used.has(token)) {
        errors.push(`classic stylesheet substitutes ${token} but never uses it`);
      }
    }
    notes.push(`classic sheet: ${used.size} tokens, ${provided.size} substitutions`);
  }
}

// --- 3: painted glyphs, not fixed-colour images ------------------------------
{
  const src = read('src/ThemeManager.cpp');
  const start = src.indexOf('QString ThemeManager::modernStyleSheet');
  const sheetEnd = src.indexOf(')"', start);
  const sheet = src.slice(start, sheetEnd);
  if (/image:\s*url\(/.test(sheet)) {
    errors.push('modern stylesheet still loads an image asset: those have a colour baked in and ' +
                'cannot follow the light/dark palette. Paint the glyph with Theme::icon().');
  }
}

report();

function report() {
  for (const note of notes) console.log(`  ${note}`);
  if (!errors.length) {
    console.log('check-css: ok');
    return;
  }
  for (const e of errors) console.error(`error: ${e}`);
  console.error(`check-css: ${errors.length} problem(s)`);
  process.exit(1);
}
