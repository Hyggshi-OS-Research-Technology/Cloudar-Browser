/* ==========================================================================
   app.js — bootstrap.

   Load order is the dependency order, and it is the whole reason this page was
   split up: nothing here has to be read in a 1100-line file to find out what
   runs when.

     app.js          bridge plumbing + chrome style, exposes CloudarNewTab
     search.js       the search form
     speed-dial.js   shortcuts
     background.js   wallpaper (IndexedDB for images and videos)
     customize.js    the panel that drives background.js

   The bridge is optional at every call site: cloudar://newtab opened outside the
   browser (or with a channel that failed to attach) must still render a working
   page, just with fewer live facts. That is why every module starts by asking
   `CloudarNewTab.bridgeReady()` instead of assuming `qt` exists.
   ========================================================================== */

var CloudarNewTab = (function () {
  'use strict';

  // What the native side told us. `null` until the channel is up, and it stays
  // null forever when the page is opened outside Cloudar.
  var state = null;
  var bridge = null;
  // True once the channel has connected or every retry has been used up, so
  // afterChannel() knows whether to call back immediately.
  var channelSettled = false;
  var pending = [];
  var readyCallbacks = [];

  function flush() {
    var queued = pending;
    pending = [];
    queued.forEach(function (fn) {
      try { fn(bridge); } catch (e) { console.error('[newtab] bridge callback failed', e); }
    });
  }

  function setState(next) {
    state = next || {};
    var style = state.chromeStyle === 'classic' ? 'classic' : 'modern';
    document.documentElement.classList.toggle('classic', style === 'classic');
    document.documentElement.setAttribute('data-chrome-style', style);
    // The classic chrome targets the machines the animations cost too much on.
    document.body.classList.toggle('reduce-motion', style === 'classic');
    document.dispatchEvent(new CustomEvent('cloudar:stylechange', { detail: style }));
  }

  // --- chrome style ------------------------------------------------------
  // Read synchronously from localStorage *before* the stylesheets paint, so the
  // dark page never flashes inside a beige window. The native side writes this
  // key on every internal page load (BrowserTabWidget::wireView); the bridge
  // then confirms it with the style actually in force for this run, which is the
  // only way --classic / CLOUDAR_THEME can be reflected here.
  (function earlyStyle() {
    try {
      var saved = localStorage.getItem('cloudar.chromeStyle');
      if (saved === 'classic') {
        document.documentElement.classList.add('classic');
        document.documentElement.setAttribute('data-chrome-style', 'classic');
        document.addEventListener('DOMContentLoaded', function () {
          document.body.classList.add('reduce-motion');
        });
      }
    } catch (e) { /* storage disabled: keep the modern look */ }
  })();

  function bridgeReady(fn) {
    if (bridge) { fn(bridge); return; }
    if (channelSettled) { fn(null); return; }
    pending.push(fn);
  }

  function onReady(fn) {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', fn, { once: true });
    } else {
      fn();
    }
  }

  function initChannel() {
    if (typeof qt === 'undefined' || !qt.webChannelTransport) {
      // No channel: the page is being viewed outside the browser, or the
      // transport has not been attached yet. Retry a few times, then give up
      // quietly — the page is fully usable without the bridge.
      if (!initChannel.tries) initChannel.tries = 0;
      initChannel.tries++;
      if (initChannel.tries < 100) {
        setTimeout(initChannel, 50);
        return;
      }
      channelSettled = true;
      readyCallbacks.forEach(function (fn) { try { fn(null); } catch (e) {} });
      readyCallbacks = [];
      return;
    }

    new QWebChannel(qt.webChannelTransport, function (channel) {
      // The New Tab page's own object. If it is missing the page still works:
      // `switchTo`/`faviconFor` below degrade to their fallbacks.
      bridge = channel.objects.newTabBridge || null;

      if (bridge) {
        // Live favicons arrive after the icon is stored, not in the reply to
        // faviconUrl() — a network round trip must not block the first paint.
        if (bridge.faviconReady) {
          bridge.faviconReady.connect(function (pageUrl, iconUrl) {
            document.dispatchEvent(new CustomEvent('cloudar:favicon', {
              detail: { pageUrl: pageUrl, iconUrl: iconUrl }
            }));
          });
        }
        try {
          var raw = bridge.initialState();
          if (raw) setState(raw);
        } catch (e) {
          console.warn('[newtab] initialState() failed', e);
        }
      }

      channelSettled = true;
      flush();
      readyCallbacks.forEach(function (fn) { try { fn(bridge); } catch (e) {} });
      readyCallbacks = [];
    });
  }

  return {
    // --- accessors -------------------------------------------------------
    bridge: function () { return bridge; },
    state: function () { return state || {}; },

    // True once the channel is up *or* has definitively failed, so callers never
    // wait forever for a bridge that will not arrive.
    bridgeReady: bridgeReady,
    onReady: onReady,

    // --- helpers ---------------------------------------------------------
    navigate: function (url) {
      if (!url) return;
      if (bridge && bridge.openUrl) {
        // Through the browser, so an internal page focuses its existing tab
        // instead of opening a second copy.
        bridge.openUrl(url);
        return;
      }
      window.location.href = url;
    },

    // Returns "" when no icon is cached. Callers must treat that as "use the
    // letter fallback or try /favicon.ico", never as an error.
    faviconFor: function (url) {
      if (!bridge || !bridge.faviconUrl) return '';
      try { return bridge.faviconUrl(url) || ''; } catch (e) { return ''; }
    },

    searchName: function () { return (state && state.searchName) || ''; },
    searchTemplate: function () {
      return (state && state.searchTemplate) || 'https://duckduckgo.com/?q=%s';
    },
    chromeStyle: function () {
      return document.documentElement.getAttribute('data-chrome-style') || 'modern';
    },

    // Escape helper used by every module that touches innerHTML.
    escapeHtml: function (s) {
      return String(s == null ? '' : s)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
    },

    hostOf: function (url) {
      try { return new URL(url).hostname; } catch (e) { return ''; }
    },

    // Runs `fn(bridgeOrNull)` once the channel has either connected or given up.
    // Separate from bridgeReady() because that one runs immediately on a
    // connected channel and never fires on a page with no channel at all.
    // Fires once the channel is up (or has definitively failed). If it is
    // already up, fires immediately — otherwise a module that loads after the
    // channel opened would wait forever.
    afterChannel: function (fn) {
      if (channelSettled) {
        try { fn(bridge); } catch (e) { console.error('[newtab] channel callback failed', e); }
        return;
      }
      readyCallbacks.push(fn);
    },
    startChannel: initChannel
  };
})();

// The four built-in tiles are plain links to internal pages. Going through
// CloudarNewTab.navigate() means the browser focuses an already-open
// cloudar://settings tab instead of stacking a second one.
function wireTiles() {
  var tiles = document.querySelectorAll('.tile');
  for (var i = 0; i < tiles.length; i++) {
    (function (tile) {
      tile.addEventListener('click', function () {
        var target = tile.getAttribute('data-target');
        if (target) CloudarNewTab.navigate(target);
      });
    })(tiles[i]);
  }
}

CloudarNewTab.onReady(wireTiles);

// The channel is opened after DOMContentLoaded so every module script below has
// registered its bridge calls first. If a module is somehow still late,
// bridgeReady() queues it instead of dropping it.
CloudarNewTab.onReady(function () {
  if (typeof QWebChannel === 'undefined') {
    // qwebchannel.js failed to load; announce it once rather than retrying a
    // name that will never appear.
    document.dispatchEvent(new CustomEvent('cloudar:channel-unavailable'));
    return;
  }
  CloudarNewTab.startChannel();
});
