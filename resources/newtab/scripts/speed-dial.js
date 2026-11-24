/* ==========================================================================
   speed-dial.js — shortcut state and persistence.

   Deliberately separate from components/quick-access.js, which owns the DOM:
   the list, the migration from the old storage key and the URL normalisation
   are testable facts, while the rendering changes with the chrome.
   ========================================================================== */

var CloudarSpeedDialStore = (function () {
  'use strict';

  var KEY = 'cloudar.newtab.speeddial';
  // The page used to key its shortcuts under the project's old name. Read it
  // once and migrate rather than dropping everyone's customised dial.
  var LEGACY_KEY = 'lightbrowser.newtab.speeddial';

  var DEFAULT_DIAL = [
    { id: 'sd-1', label: 'GitHub', url: 'https://github.com/' },
    { id: 'sd-2', label: 'YouTube', url: 'https://youtube.com/' },
    { id: 'sd-3', label: 'DuckDuckGo', url: 'https://duckduckgo.com/' }
  ];

  var AVATAR_COLORS = ['#2d7ff9', '#7a5cff', '#00b894', '#e17055',
                       '#d63384', '#0984e3', '#e67e22', '#20c997'];

  var list = [];

  function normalizeUrl(raw) {
    raw = (raw || '').trim();
    if (!raw) return '';
    if (!/^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//.test(raw)) raw = 'https://' + raw;
    try { return new URL(raw).href; } catch (e) { return ''; }
  }

  function sanitize(entries) {
    if (!Array.isArray(entries)) return null;
    var seen = Object.create(null);
    var out = [];
    entries.forEach(function (entry) {
      if (!entry || !entry.url) return;
      var url = normalizeUrl(entry.url);
      if (!url) return;
      var id = String(entry.id || ('sd-' + out.length + '-' + Date.now()));
      if (seen[id]) id = id + '-dup' + out.length;
      seen[id] = true;
      out.push({
        id: id,
        label: String(entry.label || CloudarNewTab.hostOf(url) || url),
        url: url
      });
    });
    return out;
  }

  function load() {
    var raw = null;
    try {
      raw = localStorage.getItem(KEY);
      if (!raw) {
        var legacy = localStorage.getItem(LEGACY_KEY);
        if (legacy) {
          try { localStorage.setItem(KEY, legacy); } catch (e) {}
          try { localStorage.removeItem(LEGACY_KEY); } catch (e) {}
          raw = legacy;
        }
      }
      if (raw) {
        var parsed = sanitize(JSON.parse(raw));
        if (parsed) { list = parsed; return list; }
      }
    } catch (e) {
      console.warn('[speed-dial] could not read saved shortcuts', e);
    }
    list = DEFAULT_DIAL.slice();
    return list;
  }

  function save() {
    try {
      localStorage.setItem(KEY, JSON.stringify(list));
    } catch (e) {
      // Say it out loud: the old code logged and moved on, so the shortcut
      // silently vanished on the next New Tab.
      console.warn('[speed-dial] could not save shortcuts:', e && e.name);
      document.dispatchEvent(new CustomEvent('cloudar:storage-error', {
        detail: { what: 'shortcuts', error: e && e.name }
      }));
    }
  }

  function hashColor(str) {
    var hash = 0;
    for (var i = 0; i < str.length; i++) hash = (hash * 31 + str.charCodeAt(i)) >>> 0;
    return AVATAR_COLORS[hash % AVATAR_COLORS.length];
  }

  return {
    all: function () { return list; },
    load: load,
    save: save,
    add: function (label, rawUrl) {
      var url = normalizeUrl(rawUrl);
      if (!url) return null;
      var entry = {
        id: 'sd-' + Date.now(),
        label: (label || '').trim() || CloudarNewTab.hostOf(url) || url,
        url: url
      };
      list.push(entry);
      save();
      return entry;
    },
    remove: function (id) {
      list = list.filter(function (x) { return x.id !== id; });
      save();
    },
    move: function (fromId, toId) {
      var from = -1, to = -1;
      for (var i = 0; i < list.length; i++) {
        if (list[i].id === fromId) from = i;
        if (list[i].id === toId) to = i;
      }
      if (from === -1 || to === -1 || from === to) return false;
      var moved = list.splice(from, 1)[0];
      list.splice(to, 0, moved);
      save();
      return true;
    },
    normalizeUrl: normalizeUrl,
    hashColor: hashColor
  };
})();
