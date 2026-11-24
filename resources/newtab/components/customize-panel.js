/* ==========================================================================
   customize-panel.js — small panel behaviours that are not about the wallpaper.

   Today that is the "recent history" strip: it shows what the browser reports
   (newTabBridge.initialState().recent), with the same real-favicon treatment as
   the speed dial. When there is no history the strip stays hidden rather than
   rendering a placeholder claiming there is something to continue.
   ========================================================================== */

(function () {
  'use strict';

  function iconFor(url, imgEl) {
    var cached = CloudarNewTab.faviconFor(url);
    if (cached) { imgEl.src = cached; return; }
    var host = CloudarNewTab.hostOf(url);
    if (host) {
      imgEl.src = 'https://' + host + '/favicon.ico';
      imgEl.addEventListener('error', function () { imgEl.style.visibility = 'hidden'; },
                             { once: true });
    } else {
      imgEl.style.visibility = 'hidden';
    }
  }

  function renderRecent() {
    var state = CloudarNewTab.state();
    var recent = state.recent || [];
    var wrap = document.getElementById('recent');
    var inner = document.getElementById('recentInner');
    if (!wrap || !inner) return;

    // Drop entries that point at internal pages: "continue browsing" showing
    // cloudar://settings is noise, not a page you were reading.
    recent = recent.filter(function (item) {
      return item && item.url && !/^(cloudar|browser|light):\/\//i.test(item.url);
    });

    if (!recent.length) {
      wrap.classList.remove('show');
      return;
    }

    inner.innerHTML = '';
    recent.forEach(function (item) {
      var el = document.createElement('div');
      el.className = 'recent-item';
      el.title = item.title || item.url;

      var img = document.createElement('img');
      img.alt = '';
      img.width = 14;
      img.height = 14;
      iconFor(item.url, img);

      var label = document.createElement('span');
      label.textContent = item.title || CloudarNewTab.hostOf(item.url) || item.url;

      el.appendChild(img);
      el.appendChild(label);
      el.addEventListener('click', function () { CloudarNewTab.navigate(item.url); });
      inner.appendChild(el);
    });
    wrap.classList.add('show');
  }

  // A favicon that arrives later should patch the matching thumbnail too.
  document.addEventListener('cloudar:favicon', function (e) {
    var detail = e.detail || {};
    if (!detail.iconUrl) return;
    var inner = document.getElementById('recentInner');
    var state = CloudarNewTab.state();
    if (!inner) return;
    var items = inner.querySelectorAll('.recent-item');
    (state.recent || []).forEach(function (item, i) {
      if (!item || item.url !== detail.pageUrl) return;
      var img = items[i] && items[i].querySelector('img');
      if (img) { img.style.visibility = 'visible'; img.src = detail.iconUrl; }
    });
  });

  CloudarNewTab.afterChannel(renderRecent);
})();
