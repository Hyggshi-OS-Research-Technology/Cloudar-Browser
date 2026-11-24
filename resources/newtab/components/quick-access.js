/* ==========================================================================
   quick-access.js — the speed dial DOM.

   The state, the migration from the old storage key and the URL rules live in
   scripts/speed-dial.js; this file only draws. That split is what makes the
   favicon work below possible without re-reading the list on every icon:

   1. **Real favicons.** The tile used to be a coloured square holding the first
      letter of the site name, forever. It now asks the browser
      (newTabBridge.faviconUrl) for the icon Chromium already decoded for that
      host, falls back to https://<host>/favicon.ico, and only then keeps the
      letter. The letter is a fallback, not the design.
   2. **Navigation through the browser**, so a shortcut to cloudar://settings
      focuses the open tab instead of stacking a duplicate.
   3. **Icons patch in place** when they arrive late, so a tile the user is
      dragging is never torn out from under the pointer.
   ========================================================================== */

var CloudarQuickAccess = (function () {
  'use strict';

  var tilesEl = null;
  var overlay = null;
  var nameInput = null;
  var urlInput = null;
  var dragSourceId = null;
  // id -> icon url, filled asynchronously and kept across re-renders.
  var icons = Object.create(null);

  function letterOf(entry) {
    var source = entry.label || entry.url || '?';
    return (source.trim().charAt(0) || '?').toUpperCase();
  }

  // --- icons --------------------------------------------------------------
  function applyIcon(faviconEl, url, entry) {
    if (!faviconEl || !url) return;
    faviconEl.classList.add('real');
    faviconEl.style.background = 'transparent';
    faviconEl.innerHTML = '';
    var img = document.createElement('img');
    img.src = url;
    img.alt = '';
    img.loading = 'lazy';
    img.addEventListener('error', function () {
      // A cached PNG can vanish (cache cleared, profile switched) after the tile
      // rendered; fall back rather than leaving a broken-image box.
      delete icons[entry.id];
      resetToLetter(faviconEl, entry);
    }, { once: true });
    faviconEl.appendChild(img);
  }

  function resetToLetter(faviconEl, entry) {
    faviconEl.classList.remove('real');
    faviconEl.style.background = CloudarSpeedDialStore.hashColor(entry.url);
    faviconEl.textContent = letterOf(entry);
  }

  function resolveIcon(entry, faviconEl) {
    if (icons[entry.id]) { applyIcon(faviconEl, icons[entry.id], entry); return; }

    var cached = CloudarNewTab.faviconFor(entry.url);
    if (cached) {
      icons[entry.id] = cached;
      applyIcon(faviconEl, cached, entry);
      return;
    }

    // Nothing cached yet. Ask the host directly — one request per tile without
    // an icon, which is what every speed dial does.
    var host = CloudarNewTab.hostOf(entry.url);
    if (!host || /^(cloudar|browser|light)$/i.test(entry.url.split(':')[0])) return;

    var probe = new Image();
    probe.referrerPolicy = 'no-referrer';
    probe.onload = function () {
      // Sites that answer /favicon.ico with a 1x1 spacer or a placeholder logo
      // are not serving an icon; keep the letter instead.
      if (probe.naturalWidth < 8 || probe.naturalHeight < 8) return;
      icons[entry.id] = 'https://' + host + '/favicon.ico';
      applyIcon(faviconEl, icons[entry.id], entry);
    };
    probe.onerror = function () { /* keep the letter */ };
    probe.src = 'https://' + host + '/favicon.ico';
  }

  // --- render -------------------------------------------------------------
  function buildTile(entry) {
    var tile = document.createElement('div');
    tile.className = 'speed-tile';
    tile.draggable = true;
    tile.dataset.id = entry.id;

    var favicon = document.createElement('div');
    favicon.className = 'favicon';
    favicon.style.background = CloudarSpeedDialStore.hashColor(entry.url);
    favicon.textContent = letterOf(entry);

    var label = document.createElement('div');
    label.className = 'label';
    label.textContent = entry.label;
    label.title = entry.url;

    var removeBtn = document.createElement('div');
    removeBtn.className = 'remove-btn';
    removeBtn.textContent = '\u2715';
    removeBtn.title = 'Remove';
    removeBtn.addEventListener('click', function (e) {
      e.stopPropagation();
      CloudarSpeedDialStore.remove(entry.id);
      delete icons[entry.id];
      render();
    });

    tile.appendChild(favicon);
    tile.appendChild(label);
    tile.appendChild(removeBtn);

    tile.addEventListener('click', function () {
      CloudarNewTab.navigate(entry.url);
    });

    tile.addEventListener('dragstart', function (e) {
      dragSourceId = entry.id;
      tile.classList.add('dragging');
      e.dataTransfer.effectAllowed = 'move';
      // QtWebEngine cancels a drag with no payload, same as Firefox.
      try { e.dataTransfer.setData('text/plain', entry.id); } catch (err) {}
    });
    tile.addEventListener('dragend', function () {
      tile.classList.remove('dragging');
      dragSourceId = null;
    });
    tile.addEventListener('dragover', function (e) {
      e.preventDefault();
      tile.classList.add('drag-over');
    });
    tile.addEventListener('dragleave', function () { tile.classList.remove('drag-over'); });
    tile.addEventListener('drop', function (e) {
      e.preventDefault();
      tile.classList.remove('drag-over');
      if (!dragSourceId || dragSourceId === entry.id) return;
      if (CloudarSpeedDialStore.move(dragSourceId, entry.id)) render();
    });

    resolveIcon(entry, favicon);
    return tile;
  }

  function render() {
    if (!tilesEl) return;
    tilesEl.innerHTML = '';
    var list = CloudarSpeedDialStore.all();
    list.forEach(function (entry) { tilesEl.appendChild(buildTile(entry)); });

    var addTile = document.createElement('div');
    addTile.className = 'speed-tile add-tile';
    addTile.innerHTML = '<div class="favicon">+</div><div class="label">Add</div>';
    addTile.title = 'Add a site';
    addTile.addEventListener('click', openDialog);
    tilesEl.appendChild(addTile);
  }

  function openDialog() {
    if (!overlay) return;
    nameInput.value = '';
    urlInput.value = '';
    overlay.classList.add('open');
    nameInput.focus();
  }

  function commitDialog() {
    var url = CloudarSpeedDialStore.normalizeUrl(urlInput.value);
    if (!url) { urlInput.focus(); return; }
    CloudarSpeedDialStore.add(nameInput.value, url);
    render();
    overlay.classList.remove('open');
  }

  return {
    init: function () {
      tilesEl = document.getElementById('speedTiles');
      overlay = document.getElementById('addOverlay');
      nameInput = document.getElementById('addName');
      urlInput = document.getElementById('addUrl');

      CloudarSpeedDialStore.load();
      render();

      var cancel = document.getElementById('addCancel');
      var confirm = document.getElementById('addConfirm');
      if (cancel) cancel.addEventListener('click', function () { overlay.classList.remove('open'); });
      if (confirm) confirm.addEventListener('click', commitDialog);
      if (overlay) {
        overlay.addEventListener('click', function (e) {
          if (e.target === overlay) overlay.classList.remove('open');
        });
      }
      if (urlInput) {
        urlInput.addEventListener('keydown', function (e) {
          if (e.key === 'Enter') { e.preventDefault(); commitDialog(); }
        });
      }
      if (nameInput) {
        nameInput.addEventListener('keydown', function (e) {
          if (e.key === 'Enter') { e.preventDefault(); urlInput.focus(); }
        });
      }
      document.addEventListener('cloudar:storage-error', function (e) {
        var detail = e.detail || {};
        if (detail.what !== 'shortcuts') return;
        var status = document.getElementById('bgStatus');
        if (status) {
          status.textContent = 'Could not save the shortcut (' +
            (detail.error || 'storage full') + '). It will be gone after a reload.';
          status.classList.add('warn');
        }
      });

      // An icon fetched in the background: patch the tile in place instead of
      // re-rendering, so a tile mid-drag is not lost.
      document.addEventListener('cloudar:favicon', function (e) {
        var detail = e.detail || {};
        if (!detail.iconUrl) return;
        CloudarSpeedDialStore.all().forEach(function (entry) {
          var entryUrl = entry.url.replace(/#.*$/, '');
          if (entryUrl !== detail.pageUrl && entry.url !== detail.pageUrl) return;
          icons[entry.id] = detail.iconUrl;
          var tile = tilesEl ? tilesEl.querySelector('[data-id="' + entry.id + '"]') : null;
          if (tile) applyIcon(tile.querySelector('.favicon'), detail.iconUrl, entry);
        });
      });
    }
  };
})();

CloudarNewTab.onReady(function () { CloudarQuickAccess.init(); });
