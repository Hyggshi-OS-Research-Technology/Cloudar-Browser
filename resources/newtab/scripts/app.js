/* ==========================================================================
   app.js — bootstrap and the only place that talks to the browser.

   ---------------------------------------------------------------------------
   Why every bridge call in this file takes a callback
   ---------------------------------------------------------------------------
   qwebchannel.js does not return slot values. Its generated method wrapper is
   (verbatim, from qwebchannel 6.2 / Qt 6):

       object[methodName] = function() {
           ...
           webChannel.exec({... "args": args}, function(response) {
               if (response !== undefined) {
                   var result = object.unwrapQObject(response);
                   if (callback) (callback)(result);
               }
           });
           return result;            // <- result is still undefined here
       };

   The trailing function argument a caller passes is stripped out of `args` and
   used as `callback`; the slot's return value is delivered *there*, on a later
   turn of the event loop. `return result` runs before `response` arrives, so a
   synchronous `var s = bridge.initialState()` is always `undefined`.

   That single mistake is why two separate things looked broken:
     - the New Tab page never received chromeStyle, search engine, recent history
       or the saved wallpaper, so every feature that depended on them silently
       fell back or did nothing;
     - `faviconUrl()` returned undefined for every tile, so the speed dial never
       showed a real icon even after the native side had stored one.

   Everything below is therefore callback-first, and `CloudarNewTab.whenReady()`
   is the one entry point modules use: it exists so nobody has to know whether
   the channel has connected yet.
   ========================================================================== */

var CloudarNewTab = (function () {
  'use strict';

  var state = null;          // whatever initialState() delivered, or {}
  var bridge = null;
  var channelSettled = false;
  var readyCallbacks = [];   // run once, after the channel + style are settled
  var channelCallbacks = []; // run once, when the channel is up or has failed

  // --- chrome style ------------------------------------------------------
  // Set from localStorage before the stylesheets paint (the inline <script> in
  // index.html) so the classic chrome never flashes dark. The bridge then
  // *corrects* it: only the browser knows about --classic / CLOUDAR_THEME, and
  // those deliberately never touch the saved preference.
  function applyStyle(styleKey) {
    var classic = styleKey === 'classic';
    document.documentElement.classList.toggle('classic', classic);
    document.documentElement.setAttribute('data-chrome-style', classic ? 'classic' : 'modern');
    // The classic chrome is chosen for machines where the animation costs too
    // much; on this page that means the drifting gradient and floating blobs.
    document.body.classList.toggle('reduce-motion', classic);
    document.dispatchEvent(new CustomEvent('cloudar:stylechange', {
      detail: classic ? 'classic' : 'modern'
    }));
  }

  function currentStyle() {
    return document.documentElement.getAttribute('data-chrome-style') || 'modern';
  }

  // --- helpers -----------------------------------------------------------
  function fail(channel, e) {
    console.error('[newtab] bridge call failed', e);
    if (channel) notifyChannel(null);
  }

  function notifyChannel(value) {
    if (channelSettled)
      return;
    channelSettled = true;
    bridge = value;
    var queued = channelCallbacks;
    channelCallbacks = [];
    queued.forEach(function (fn) {
      try { fn(bridge); } catch (e) { console.error('[newtab] channel callback failed', e); }
    });
  }

  function notifyReady() {
    var queued = readyCallbacks;
    readyCallbacks = [];
    queued.forEach(function (fn) {
      try { fn(state || {}); } catch (e) { console.error('[newtab] ready callback failed', e); }
    });
  }

  function onReady(fn) {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', fn, { once: true });
    } else {
      fn();
    }
  }

  // --- the channel -------------------------------------------------------
  function startChannel() {
    var attempts = 0;

    function attempt() {
      if (typeof qt === 'undefined' || !qt.webChannelTransport) {
        attempts++;
        // Opened outside Cloudar, or the transport is not attached yet. Give up
        // after ~5 s and let the page run in its no-bridge mode rather than
        // retrying forever.
        if (attempts < 100) {
          setTimeout(attempt, 50);
          return;
        }
        notifyChannel(null);
        notifyReady();
        return;
      }

      new QWebChannel(qt.webChannelTransport, function (channel) {
        var api = channel.objects.newTabBridge;

        if (!api) {
          // The object is registered by MainWindow; if it is missing this is an
          // older build and the page still has to work.
          notifyChannel(null);
          notifyReady();
          return;
        }

        // Live favicons arrive after the bytes are stored, so a tile can update
        // without a reload. Connect before asking for anything.
        if (api.faviconReady) {
          api.faviconReady.connect(function (pageUrl, iconUrl) {
            document.dispatchEvent(new CustomEvent('cloudar:favicon', {
              detail: { pageUrl: pageUrl || '', iconUrl: iconUrl || '' }
            }));
          });
        }

        // initialState is the one blocking dependency: the style, the search
        // engine, recent history and the wallpaper all come from it, so the
        // ready callbacks wait for it. Everything else is fire-and-forget.
        try {
          api.initialState(function (data) {
            state = data || {};
            if (state.chromeStyle)
              applyStyle(state.chromeStyle);
            notifyChannel(api);
            notifyReady();
          });
        } catch (e) {
          notifyChannel(api);
          notifyReady();
        }
      });
    }

    attempt();
  }

  return {
    // --- lifecycle -------------------------------------------------------
    onReady: onReady,
    startChannel: startChannel,

    // Runs `fn(state)` once the channel has answered (or failed) and the style
    // is in force. Always async — even when the channel is already up, so a
    // module can be written without caring which happened first.
    whenReady: function (fn) {
      if (channelSettled && state !== null) {
        try { fn(state); } catch (e) { console.error('[newtab] ready callback failed', e); }
        return;
      }
      readyCallbacks.push(fn);
    },

    // Runs `fn(bridgeOrNull)` once the channel is up or has definitively failed.
    // Kept for modules that only need the API object.
    afterChannel: function (fn) {
      if (channelSettled) {
        try { fn(bridge); } catch (e) { console.error('[newtab] channel callback failed', e); }
        return;
      }
      channelCallbacks.push(fn);
    },

    state: function () { return state || {}; },
    bridge: function () { return bridge; },
    chromeStyle: currentStyle,

    // --- browser actions -------------------------------------------------
    navigate: function (url) {
      if (!url)
        return;
      if (!bridge || !bridge.openUrl) {
        // No channel: a plain navigation is the only option left.
        window.location.href = url;
        return;
      }
      bridge.openUrl(url);
    },

    // Async by necessity: the icon may need to be read off disk or fetched from
    // the site first. `cb(iconUrl)` receives "" when there is nothing cached.
    faviconFor: function (url, cb) {
      var done = typeof cb === 'function' ? cb : function () {};
      var pageUrl = String(url || '').replace(/#.*$/, '');
      if (!bridge || !bridge.faviconUrl) {
        done('');
        return;
      }
      try {
        bridge.faviconUrl(pageUrl, function (iconUrl) { done(iconUrl || ''); });
      } catch (e) {
        done('');
      }
    },

    // Mirrors a wallpaper choice into QSettings + AppDataLocation. `cb(ok)` is
    // optional; the caller uses it to put a real message in the panel.
    // `mime` matters for an uploaded image: the browser keeps exactly one file
    // per kind, and its extension and Content-Type have to describe the bytes it
    // actually received. See prepareImage() in scripts/background.js.
    saveMedia: function (kind, blobOrNull, value, mime, cb) {
      // Called as saveMedia(kind, blob, value, cb) by older call sites: only a
      // string in the mime position can be a type.
      if (typeof mime === 'function') { cb = mime; mime = ''; }
      var done = typeof cb === 'function' ? cb : function () {};
      if (!bridge || !bridge.saveWallpaperMedia) {
        done(false, 'no bridge');
        return;
      }
      var api = bridge;

      function send(base64) {
        try {
          api.saveWallpaperMedia(kind, base64 || '', value || '', mime || '', function (url) {
            done(!!url || kind === 'color' || kind === 'web', url || '');
          });
        } catch (e) {
          done(false, e && e.name);
        }
      }

      if (!blobOrNull) { send(''); return; }

      var reader = new FileReader();
      reader.onerror = function () { done(false, (reader.error && reader.error.name) || 'read error'); };
      reader.onload = function () {
        var raw = String(reader.result || '');
        var comma = raw.indexOf(',');
        send(comma >= 0 ? raw.slice(comma + 1) : '');
      };
      reader.readAsDataURL(blobOrNull);
    },

    // Where the wallpaper sits in the window. Same rule as every other
    // preference here: the browser stores it (QSettings), not the page, because
    // the page's storage is cleared with browsing data.
    saveFraming: function (x, y, zoom, cb) {
      var done = typeof cb === 'function' ? cb : function () {};
      if (!bridge || !bridge.saveWallpaperFraming) { done(false, 'no bridge'); return; }
      try {
        bridge.saveWallpaperFraming(Number(x) || 0, Number(y) || 0, Number(zoom) || 1,
          function (ok) { done(!!ok, ok ? '' : 'the browser refused it'); });
      } catch (err) {
        done(false, err && err.name);
      }
    },

    clearMedia: function (cb) {
      var done = typeof cb === 'function' ? cb : function () {};
      if (!bridge || !bridge.clearWallpaperMedia) { done(); return; }
      try {
        bridge.clearWallpaperMedia(function () { done(); });
      } catch (e) { done(); }
    },

    // --- small shared utilities -------------------------------------------
    searchName: function () { return (state && state.searchName) || ''; },
    searchTemplate: function () {
      return (state && state.searchTemplate) || 'https://duckduckgo.com/?q=%s';
    },

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
    }
  };
})();

// The four built-in tiles are plain links to internal pages. Going through
// CloudarNewTab.navigate() means the browser focuses an already-open
// cloudar://settings tab instead of stacking a second one.
CloudarNewTab.onReady(function () {
  var tiles = document.querySelectorAll('.tile');
  for (var i = 0; i < tiles.length; i++) {
    (function (tile) {
      tile.addEventListener('click', function () {
        var target = tile.getAttribute('data-target');
        if (target) CloudarNewTab.navigate(target);
      });
    })(tiles[i]);
  }
});

CloudarNewTab.onReady(function () {
  if (typeof QWebChannel === 'undefined') {
    // qwebchannel.js did not load; announce it once instead of retrying a name
    // that will never appear.
    document.dispatchEvent(new CustomEvent('cloudar:channel-unavailable'));
    return;
  }
  CloudarNewTab.startChannel();
});
