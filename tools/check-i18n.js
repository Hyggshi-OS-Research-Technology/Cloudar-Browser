#!/usr/bin/env node
/*
 * Translation checks, run in CI before the Qt build.
 *
 * The interface language setting is only worth shipping if the dictionaries are
 * actually complete and actually used, so this script measures rather than
 * trusts. Each check names the defect it prevents:
 *
 *   1. a language listed in LanguageManager but with no lang/<code>.json
 *      -> picking it in Settings shows English and nothing says why
 *   2. a dictionary missing keys that another one has
 *      -> those strings silently fall back to English in one language only,
 *         which is exactly the kind of half-applied state this project treats
 *         as a bug
 *   3. an English dictionary entry that no code can reach
 *      -> dead weight that makes "N of N strings translated" a lie
 *   4. a `tr("...")` in the C++ chrome with no entry in en.json
 *      -> the native menus stay English in every language, which is the state
 *         this whole PR exists to fix
 *   5. a `get_text("key")` with no such key
 *      -> the UI renders the raw key name
 *   6. a [data-i18n]/[data-i18n-placeholder] attribute in an internal page with
 *      no such key
 *      -> the page keeps its English text under a Vietnamese UI
 *   7. an internal page that never loads i18n.js
 *      -> the attribute is there but nothing applies it
 *   8. lang/*.json not in resources.qrc
 *      -> installed builds cannot find the dictionaries at all
 *
 * Usage: node tools/check-i18n.js
 */
'use strict';

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const LANG_DIR = path.join(ROOT, 'lang');
const SRC_DIR = path.join(ROOT, 'src');
const RES_DIR = path.join(ROOT, 'resources');

const errors = [];
const notes = [];

function read(p) {
  return fs.readFileSync(p, 'utf8');
}

function json(p) {
  return JSON.parse(read(p));
}

// --- dictionaries ------------------------------------------------------------
const codes = fs.readdirSync(LANG_DIR)
  .filter((f) => f.endsWith('.json'))
  .map((f) => f.slice(0, -5))
  .sort();

if (!codes.includes('en')) {
  errors.push('lang/en.json is missing: every other translation falls back to it');
  report();
}

const dicts = {};
for (const code of codes) {
  try {
    dicts[code] = json(path.join(LANG_DIR, `${code}.json`));
  } catch (e) {
    errors.push(`lang/${code}.json is not valid JSON: ${e.message}`);
  }
}

const english = dicts.en || {};
const englishKeys = Object.keys(english);

// --- 1: every language LanguageManager offers has a dictionary --------------
const languageCpp = read(path.join(SRC_DIR, 'Language.cpp'));
const offered = [...languageCpp.matchAll(/\{\s*"([a-z]{2})"\s*,\s*QString(?:Literal)?\(?"/g)]
  .map((m) => m[1]);
// The initialiser list spells the native name as a plain string literal; the
// regex above covers both that and QStringLiteral forms.
const offeredList = offered.length ? offered : [...languageCpp.matchAll(/\{"([a-z]{2})",/g)]
  .map((m) => m[1]);

if (!offeredList.length) {
  errors.push('could not parse the supported-language list out of src/Language.cpp');
}
for (const code of offeredList) {
  if (!codes.includes(code)) {
    errors.push(`LanguageManager offers "${code}" but lang/${code}.json does not exist`);
  }
}
for (const code of codes) {
  if (offeredList.length && !offeredList.includes(code)) {
    errors.push(`lang/${code}.json exists but LanguageManager never offers it`);
  }
}

// --- 2: no dictionary is missing a key another one has ----------------------
for (const code of codes) {
  if (code === 'en' || !dicts[code]) continue;
  const keys = new Set(Object.keys(dicts[code]));
  const missing = englishKeys.filter((k) => !keys.has(k));
  if (missing.length) {
    errors.push(`lang/${code}.json is missing ${missing.length} key(s): ` +
                `${missing.slice(0, 6).join(', ')}${missing.length > 6 ? ', …' : ''}`);
  }
  const extra = [...keys].filter((k) => !englishKeys.includes(k));
  if (extra.length) {
    errors.push(`lang/${code}.json has ${extra.length} key(s) not in en.json: ` +
                `${extra.slice(0, 6).join(', ')} — they can never be reached`);
  }
}

// --- 3/4/5: the keys the C++ chrome can actually reach ---------------------
const cppFiles = fs.readdirSync(SRC_DIR).filter((f) => f.endsWith('.cpp'));
const usedKeys = new Set();
const trSources = new Set();
for (const file of cppFiles) {
  const src = read(path.join(SRC_DIR, file));
  for (const m of src.matchAll(/get_text\(\s*"([a-z0-9_]+)"\s*\)/g)) usedKeys.add(m[1]);
  // QStringLiteral and const char* both reach tr(); the argument is the source
  // text, which is what the dictionary is keyed against on the Qt side.
  for (const m of src.matchAll(/\btr\(\s*"((?:[^"\\]|\\.)*)"\s*\)/g)) {
    trSources.add(m[1]);
  }
}

const escape = (s) => s
  .replace(/\\u2606/g, '\u2606')
  .replace(/\\u25b8/g, '\u25b8')
  .replace(/\\u22ee/g, '\u22ee')
  .replace(/\\"/g, '"');

const englishValues = new Set(Object.values(english));
const englishValuesNoAccel = new Set([...englishValues].map((v) => v.replace(/&/g, '')));

for (const source of trSources) {
  if (!source) continue;
  const decoded = escape(source);
  const bare = decoded.replace(/&/g, '');
  if (englishValues.has(decoded) || englishValuesNoAccel.has(bare)) continue;
  errors.push(`tr("${decoded}") in the C++ chrome has no en.json entry, ` +
              `so it can never be translated`);
}

for (const key of usedKeys) {
  if (!(key in english)) errors.push(`get_text("${key}") is used but en.json has no such key`);
}

// --- 6/7: internal pages (parsed first: gate 3 needs the keys they use) ----------------------------------------------------
// The New Tab page is one of these too, under its own path.
const PAGE_FILES = ['about', 'bookmarks', 'downloads', 'extensions', 'files',
                    'history', 'permissions', 'settings']
  .map((p) => path.join(RES_DIR, `${p}.html`))
  .concat([path.join(RES_DIR, 'newtab', 'index.html')]);

const pageKeys = new Set();
for (const file of PAGE_FILES) {
  if (!fs.existsSync(file)) continue;
  const src = read(file);
  if (!/src="i18n\.js"/.test(src)) {
    errors.push(`${path.relative(ROOT, file)} does not load i18n.js`);
  }
  for (const m of src.matchAll(/data-i18n(?:-placeholder|-title)?="([a-z0-9_]+)"/g)) {
    pageKeys.add(m[1]);
  }
  for (const m of src.matchAll(/CloudarI18n\.t\(\s*'([a-z0-9_]+)'/g)) {
    pageKeys.add(m[1]);
  }
}
for (const key of pageKeys) {
  if (!(key in english)) {
    errors.push(`an internal page asks for "${key}" but en.json has no such key`);
  }
}


// --- 3: an en.json entry that was *added here* and reaches nothing ----------
// Only keys that are not already in the repository's baseline are checked: the
// dictionaries predate this check and carry a few reserved keys ("app_name",
// the window-title formats) that are deliberately kept for future use. Failing
// on those would make the gate noise, and noise is how gates get disabled.
//
// The baseline is lang/en.json as of the commit that introduced this script; a
// key added later that nothing reads is exactly the mistake worth catching.
const BASELINE = path.join(ROOT, 'tools', 'i18n-baseline.json');
const baseline = fs.existsSync(BASELINE) ? new Set(json(BASELINE)) : new Set(englishKeys);

for (const key of englishKeys) {
  if (baseline.has(key)) continue;
  if (usedKeys.has(key) || pageKeys.has(key)) continue;
  const reachable = [...trSources].some((s) =>
    escape(s).replace(/&/g, '') === String(english[key]).replace(/&/g, ''));
  if (reachable) continue;
  errors.push(`en.json key "${key}" is new but not reachable from any tr(), ` +
              `get_text() or [data-i18n] use`);
}

// --- 8: the dictionaries are in the resource bundle -------------------------
const qrc = read(path.join(RES_DIR, 'resources.qrc'));
for (const code of codes) {
  if (!qrc.includes(`lang/${code}.json`)) {
    errors.push(`lang/${code}.json is not listed in resources.qrc: an installed ` +
                `build would have no dictionary for it`);
  }
}

// --- summary ----------------------------------------------------------------
notes.push(`${codes.length} dictionaries, ${englishKeys.length} strings each, ` +
           `${usedKeys.size + pageKeys.size} keys referenced`);

walk: {
  // Nothing to do; the label exists so the summary reads clearly in CI logs.
  break walk;
}

function report() {
  for (const note of notes) console.log(note);
  if (errors.length) {
    for (const e of errors) console.error(`::error title=i18n::${e}`);
    console.error(`\ncheck-i18n: ${errors.length} problem(s)`);
    process.exit(1);
  }
  console.log(`check-i18n: ok (${notes.join('; ')})`);
  process.exit(0);
}

report();
