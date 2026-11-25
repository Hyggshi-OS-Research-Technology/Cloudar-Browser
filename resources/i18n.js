// Cloudar internal-page translations.
//
// Every cloudar:// page is plain static HTML served from the binary, so there is
// no build step that could pre-render ten languages into it. Instead the native
// side serves the *resolved* dictionary at the same origin ("i18n.json") and this
// runtime swaps the text in place.
//
// How a page marks itself up:
//
//   <h1 data-i18n="page_history">History</h1>
//   <input data-i18n-placeholder="history_search_placeholder" placeholder="Search history">
//   <button data-i18n-title="permission_deny">Deny</button>
//
// The English text stays in the HTML as the fallback: it is what the page shows
// before the fetch resolves, and what it keeps showing if the fetch ever fails.
// Nothing is ever rendered blank. When a page builds rows from JavaScript it asks
// for the text with CloudarI18n.t("key") instead of hard-coding a string.
//
// Load order matters: this file is included with `defer` *after* the page's own
// script so that `CloudarI18n.t` exists when that script first runs. It resolves
// asynchronously and then calls the page's `cloudarLanguageApplied` hook, if any,
// so a page that renders text from script can re-render itself once.
(function () {
  'use strict';

  var dict = null;
  var ready = false;
  var waiters = [];

  function lookup(key) {
    if (dict && typeof dict[key] === 'string' && dict[key]) return dict[key];
    return null;
  }

  function t(key, fallback) {
    var value = lookup(key);
    if (value !== null) return value;
    if (typeof fallback === 'string') return fallback;
    return key;
  }

  function apply(root) {
    var scope = root || document;

    var texts = scope.querySelectorAll('[data-i18n]');
    for (var i = 0; i < texts.length; i++) {
      var key = texts[i].getAttribute('data-i18n');
      var value = lookup(key);
      if (value !== null) texts[i].textContent = value;
    }

    var placeholders = scope.querySelectorAll('[data-i18n-placeholder]');
    for (var j = 0; j < placeholders.length; j++) {
      var pKey = placeholders[j].getAttribute('data-i18n-placeholder');
      var pValue = lookup(pKey);
      if (pValue !== null) placeholders[j].setAttribute('placeholder', pValue);
    }

    var titles = scope.querySelectorAll('[data-i18n-title]');
    for (var k = 0; k < titles.length; k++) {
      var tKey = titles[k].getAttribute('data-i18n-title');
      var tValue = lookup(tKey);
      if (tValue !== null) titles[k].setAttribute('title', tValue);
    }

    // The dictionary carries the resolved code, which is what lang= wants (not
    // the user's preference: "system" is not a language).
    if (dict && dict._language) {
      document.documentElement.setAttribute('lang', dict._language);
      document.documentElement.setAttribute('data-language', dict._language);
    }
  }

  function flush() {
    ready = true;
    apply(document);
    var pending = waiters;
    waiters = [];
    for (var i = 0; i < pending.length; i++) {
      try { pending[i](dict); } catch (e) { /* a page hook must not break i18n */ }
    }
    if (typeof window.cloudarLanguageApplied === 'function') {
      try { window.cloudarLanguageApplied(dict); } catch (e) { /* same */ }
    }
    document.dispatchEvent(new CustomEvent('cloudar:i18n', { detail: dict }));
  }

  window.CloudarI18n = {
    t: t,
    // Runs `fn` once the dictionary has loaded (immediately when it already has).
    whenReady: function (fn) {
      if (ready) { fn(dict); return; }
      waiters.push(fn);
    },
    // Re-applies the attributes to a subtree, for content the page inserted later.
    apply: apply,
    get language() { return dict && dict._language ? dict._language : null; }
  };

  // `no-cache` because the dictionary changes the moment the user picks another
  // language, and this same renderer is reused across page loads.
  fetch('i18n.json', { cache: 'no-cache' })
    .then(function (response) {
      if (!response.ok) throw new Error('i18n ' + response.status);
      return response.json();
    })
    .then(function (json) {
      dict = json || {};
      flush();
    })
    .catch(function () {
      // No dictionary: the English already in the markup is the page. Still mark
      // the document as ready so page scripts that wait on it do not hang.
      dict = dict || {};
      flush();
    });
})();
