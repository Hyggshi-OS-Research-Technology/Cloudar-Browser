#include "ReaderMode.h"

QString ReaderMode::toggleScript()
{
    static const char *kScript = R"JS(
(function() {
  var existing = document.getElementById('__lbReaderOverlay');
  if (existing) {
    existing.remove();
    document.documentElement.style.overflow = '';
    return 'closed';
  }

  function textLength(el) {
    return (el.innerText || el.textContent || '').trim().length;
  }

  function linkDensity(el) {
    var linkLen = 0;
    el.querySelectorAll('a').forEach(function(a) { linkLen += textLength(a); });
    var total = textLength(el) || 1;
    return linkLen / total;
  }

  // Density-based scoring: prefer containers with several substantial
  // paragraphs and a low proportion of link text (nav/sidebar/footer tend
  // to be mostly links). This is a simplified heuristic, not a full
  // Readability-style parser, but it's enough for typical article pages.
  function scoreCandidate(el) {
    var paras = el.querySelectorAll('p');
    var goodParas = 0, paraTextLen = 0;
    paras.forEach(function(p) {
      var len = textLength(p);
      if (len > 25) { goodParas++; paraTextLen += len; }
    });
    if (goodParas === 0) return 0;
    var density = linkDensity(el);
    return paraTextLen * (1 - Math.min(density, 0.9)) + goodParas * 20;
  }

  var candidates = document.querySelectorAll('article, main, [role="main"], div, section');
  var best = null, bestScore = 0;
  candidates.forEach(function(el) {
    var s = scoreCandidate(el);
    if (s > bestScore) { bestScore = s; best = el; }
  });

  if (!best || bestScore < 150) {
    return 'no-article';
  }

  var clone = best.cloneNode(true);
  var stripSelectors = 'script, style, iframe, noscript, button, input, select, textarea, form, ' +
    'nav, aside, footer, header, svg, [role="navigation"], [class*="share"], [class*="social"], ' +
    '[class*="comment"], [class*="sidebar"], [class*="advert"], [class*="ads-"], [id*="ad-"], ' +
    '[class*="related"], [class*="newsletter"], [class*="cookie"], [class*="popup"]';
  clone.querySelectorAll(stripSelectors).forEach(function(n) { n.remove(); });
  clone.querySelectorAll('*').forEach(function(n) {
    if (!n.textContent.trim() && n.tagName !== 'IMG' && n.tagName !== 'BR' && !n.querySelector('img'))
      n.remove();
  });
  clone.querySelectorAll('[style]').forEach(function(n) { n.removeAttribute('style'); });
  clone.querySelectorAll('a').forEach(function(a) {
    a.removeAttribute('onclick');
    a.setAttribute('target', '_blank');
    a.setAttribute('rel', 'noopener noreferrer');
  });

  var ogTitle = document.querySelector('meta[property="og:title"]');
  var h1 = document.querySelector('h1');
  var title = (ogTitle && ogTitle.content) || (h1 && h1.innerText) || document.title || 'Untitled';

  var byline = '';
  var authorMeta = document.querySelector('meta[name="author"]') || document.querySelector('meta[property="article:author"]');
  if (authorMeta && authorMeta.content) byline = authorMeta.content;

  var wordCount = (clone.innerText || '').trim().split(/\s+/).filter(Boolean).length;
  var readMins = Math.max(1, Math.round(wordCount / 200));

  var overlay = document.createElement('div');
  overlay.id = '__lbReaderOverlay';
  overlay.style.cssText = 'position:fixed;inset:0;z-index:2147483647;background:#14161c;overflow:auto;';

  var closeBtn = document.createElement('button');
  closeBtn.textContent = '\u2715 Exit Reader';
  closeBtn.style.cssText = 'position:fixed;top:16px;right:20px;z-index:2;padding:8px 14px;' +
    'border-radius:20px;border:1px solid rgba(255,255,255,0.15);background:rgba(255,255,255,0.06);' +
    'color:#e8ecf5;font-size:13px;cursor:pointer;font-family:-apple-system,Segoe UI,Roboto,sans-serif;';
  closeBtn.addEventListener('click', function() {
    overlay.remove();
    document.documentElement.style.overflow = '';
  });

  var article = document.createElement('div');
  article.style.cssText = 'max-width:680px;margin:0 auto;padding:70px 24px 100px;color:#e8ecf5;' +
    'font-family:Georgia,\'Times New Roman\',serif;line-height:1.7;font-size:18px;';

  var titleEl = document.createElement('h1');
  titleEl.textContent = title;
  titleEl.style.cssText = 'font-family:-apple-system,Segoe UI,Roboto,sans-serif;font-size:30px;' +
    'font-weight:700;line-height:1.3;margin:0 0 10px;color:#fff;';
  article.appendChild(titleEl);

  var metaLine = document.createElement('div');
  var metaParts = [];
  if (byline) metaParts.push(byline);
  metaParts.push(readMins + ' min read');
  metaLine.textContent = metaParts.join(' \u00b7 ');
  metaLine.style.cssText = 'font-family:-apple-system,Segoe UI,Roboto,sans-serif;font-size:13px;' +
    'color:#8b93a7;margin-bottom:32px;';
  article.appendChild(metaLine);

  var body = document.createElement('div');
  body.innerHTML = clone.innerHTML;
  body.querySelectorAll('img').forEach(function(img) {
    img.style.cssText = 'max-width:100%;height:auto;border-radius:8px;margin:20px 0;';
  });
  body.querySelectorAll('h1,h2,h3,h4').forEach(function(h) {
    h.style.cssText = 'font-family:-apple-system,Segoe UI,Roboto,sans-serif;color:#fff;margin-top:36px;';
  });
  body.querySelectorAll('a').forEach(function(a) {
    a.style.cssText = 'color:#2d7ff9;';
  });
  body.querySelectorAll('p').forEach(function(p) {
    p.style.cssText = 'margin:0 0 20px;';
  });
  article.appendChild(body);

  overlay.appendChild(closeBtn);
  overlay.appendChild(article);
  document.body.appendChild(overlay);
  document.documentElement.style.overflow = 'hidden';
  return 'opened';
})();
)JS";
    return QString::fromUtf8(kScript);
}
