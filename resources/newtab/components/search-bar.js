/* ==========================================================================
   search-bar.js — renders the search field's engine label.

   Kept separate from scripts/search.js (which owns the submit behaviour) so the
   displayed engine and the engine actually used cannot drift: both read
   CloudarNewTab.searchTemplate(), and the label comes from the bridge's
   searchName, which is derived from that same template on the C++ side.
   ========================================================================== */

(function () {
  'use strict';

  function label() {
    var name = CloudarNewTab.searchName();
    var el = document.getElementById('searchEngine');
    if (!el) return;
    if (!name) {
      // No bridge (page opened outside the browser). Say nothing rather than
      // printing a brand the browser did not confirm.
      el.textContent = '';
      el.style.display = 'none';
      return;
    }
    el.textContent = name;
    el.title = 'Searches go to ' + name;
  }

  CloudarNewTab.afterChannel(label);
  document.addEventListener('cloudar:stylechange', label);
})();
