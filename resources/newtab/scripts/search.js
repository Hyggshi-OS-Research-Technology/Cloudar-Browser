/* ==========================================================================
   search.js — the search box.

   The engine comes from the browser's own setting (browser/searchEngine) via
   the bridge, so changing it in Settings changes it here. The old page had a
   hard-coded DuckDuckGo form and a separate hard-coded /?q= redirect, which is
   why the two could disagree after a settings change.
   ========================================================================== */

(function () {
  'use strict';

  function templateFor(mode) {
    var configured = CloudarNewTab.searchTemplate();
    if (mode !== 'lucky') return configured;
    // "I'm feeling lucky" is only meaningful for an engine that has one. For
    // anything else the first result page is the honest answer, so keep the
    // normal template rather than pretending.
    if (/duckduckgo/i.test(configured)) return 'https://duckduckgo.com/?q=%s&kf=1';
    if (/google/i.test(configured)) return 'https://www.google.com/search?btnI=1&q=%s';
    return configured;
  }

  function run(query, mode) {
    var q = (query || '').trim();
    if (!q) return;

    // A bare URL or something that looks like a domain is navigated to, not
    // searched for — the same rule the omnibox uses (MainWindow::resolveInput).
    if (/^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//.test(q)) {
      CloudarNewTab.navigate(q);
      return;
    }
    if (!/\s/.test(q) && /^[^\s/]+\.[a-zA-Z]{2,}(\/.*)?$/.test(q)) {
      CloudarNewTab.navigate('https://' + q);
      return;
    }

    var template = templateFor(mode);
    CloudarNewTab.navigate(template.replace('%s', encodeURIComponent(q)));
  }

  CloudarNewTab.onReady(function () {
    var form = document.getElementById('searchForm');
    var input = document.getElementById('searchInput');
    var engine = document.getElementById('searchEngine');
    if (!form || !input) return;

    form.addEventListener('submit', function (e) {
      e.preventDefault();
      run(input.value, 'search');
    });

    input.addEventListener('keydown', function (e) {
      // Ctrl+Enter is "I'm feeling lucky", matching real Chrome.
      if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) {
        e.preventDefault();
        run(input.value, 'lucky');
      }
    });

    CloudarNewTab.afterChannel(function () {
      var name = CloudarNewTab.searchName();
      if (engine && name) {
        // A fact reported by the browser, not a guess: empty stays empty.
        engine.textContent = name;
        engine.title = 'Searches go to ' + name +
                       ' (change it in Settings \u203a Search)';
      }
    });
  });
})();
