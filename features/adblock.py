"""
Network-level ad & tracker blocker for Cloudar Browser (tm)

This blocks outgoing requests to known ad/tracking domains at the
QtWebEngine request-interceptor level — before the request ever leaves
the process — for every site, plus a set of YouTube-specific ad and
telemetry endpoints.

IMPORTANT — what this can and cannot do on YouTube:
YouTube increasingly serves some ads *stitched into the same media
manifest as the video itself* (server-side ad insertion / SSAI), so
there is no separate network request to block for those. This module
still blocks the separate ad-request/telemetry calls the player makes
(pagead, ptracking, stats/ads, doubleclick, 2mdn, ...), which covers
most banner/companion/masthead ads and a good share of preroll/midroll
requests. The remaining in-stream ads are handled cosmetically /
behaviorally by the content-script extension instead, see:
    features/extensions/adblock/background.js
    (auto-clicks the Skip button, hides ad overlays/banners, mutes
    non-skippable ads while they play)

Wiring (already present in core/browser_window.py):
    self.adblock = AdBlockInterceptor(self)
    self.adblock.set_enabled(self.settings.get("adblock_enabled", True))
    self.profile.setUrlRequestInterceptor(self.adblock)
    self.adblock.blocked_count_changed.connect(self._on_adblock_count_changed)
"""
import threading

try:
    from core.browser_qt import QObject, pyqtSignal, QWebEngineUrlRequestInterceptor
except ImportError:  # pragma: no cover - fallback if not re-exported there
    try:
        from PyQt6.QtCore import QObject, pyqtSignal
        from PyQt6.QtWebEngineCore import QWebEngineUrlRequestInterceptor
    except ImportError:
        from PyQt5.QtCore import QObject, pyqtSignal
        from PyQt5.QtWebEngineCore import QWebEngineUrlRequestInterceptor


# ── General ad / tracker domains ─────────────────────────────────────────
# Intentionally a curated "80/20" list rather than a full EasyList mirror:
# small enough to match on every single request with negligible overhead,
# while covering the ad/analytics networks that show up on the vast
# majority of sites.
GENERAL_AD_DOMAINS = {
    "doubleclick.net", "googlesyndication.com", "googleadservices.com",
    "adservice.google.com", "adservice.google.co.uk", "google-analytics.com",
    "googletagmanager.com", "googletagservices.com", "google.com/pagead",
    "adnxs.com", "advertising.com", "adsafeprotected.com", "moatads.com",
    "scorecardresearch.com", "outbrain.com", "taboola.com", "criteo.com",
    "criteo.net", "rubiconproject.com", "pubmatic.com", "openx.net",
    "adform.net", "amazon-adsystem.com", "media.net", "adsrvr.org",
    "bidswitch.net", "casalemedia.com", "contextweb.com", "yieldmo.com",
    "3lift.com", "sharethrough.com", "quantserve.com", "chartbeat.com",
    "hotjar.com", "mixpanel.com", "segment.io", "connect.facebook.net",
    "analytics.twitter.com", "ads-twitter.com", "bat.bing.com", "clarity.ms",
    "adroll.com", "mediamath.com", "gumgum.com", "indexww.com", "smartadserver.com",
    "teads.tv", "spotxchange.com", "adcolony.com", "vungle.com", "unityads.unity3d.com",
}

# YouTube-specific ad/telemetry endpoints. This does NOT (and cannot at
# the network level) remove ads that are stitched into the video stream
# itself; it blocks the separate ad-request/telemetry calls the player
# makes, plus static/companion/display ad assets.
YOUTUBE_AD_DOMAINS = {
    "googleads.g.doubleclick.net", "static.doubleclick.net", "s0.2mdn.net",
    "2mdn.net", "youtube.com/api/stats/ads", "youtube.com/pagead",
    "youtube.com/ptracking", "youtube.com/get_midroll_info",
    "www.youtube.com/api/stats/ads", "www.youtube.com/pagead",
    "www.youtube.com/ptracking",
}

_ALL_DOMAINS = GENERAL_AD_DOMAINS | YOUTUBE_AD_DOMAINS
# Split into "bare domain" matches (host equality / suffix) vs
# "domain+path substring" matches (e.g. "youtube.com/pagead") up front,
# so interceptRequest doesn't re-split strings on every single request.
_DOMAIN_ONLY = frozenset(d for d in _ALL_DOMAINS if "/" not in d)
_DOMAIN_PATH = tuple(d for d in _ALL_DOMAINS if "/" in d)


class AdBlockInterceptor(QWebEngineUrlRequestInterceptor):
    """Blocks outgoing requests whose host (or host+path) matches a known
    ad/tracking domain. Emits `blocked_count_changed(total_blocked)` so
    the UI can show a running counter in the status bar."""

    blocked_count_changed = pyqtSignal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._enabled = True
        self._count = 0
        self._lock = threading.Lock()
        # Hostnames (without "www.") where the user chose to disable
        # AdBlock via a per-site toggle, e.g. from the toolbar shield icon.
        self._allowlist_hosts = set()

    # ── Public API (called from browser_window.py / settings UI) ───────
    def set_enabled(self, enabled: bool):
        self._enabled = bool(enabled)

    def is_enabled(self) -> bool:
        return self._enabled

    def blocked_count(self) -> int:
        return self._count

    def reset_count(self):
        with self._lock:
            self._count = 0
        self.blocked_count_changed.emit(0)

    def set_allowlist(self, hosts):
        """hosts: iterable of hostnames where AdBlock should be skipped
        entirely (network-level) for this session."""
        self._allowlist_hosts = {self._bare_host(h) for h in hosts if h}

    def add_to_allowlist(self, host):
        self._allowlist_hosts.add(self._bare_host(host))

    def remove_from_allowlist(self, host):
        self._allowlist_hosts.discard(self._bare_host(host))

    @staticmethod
    def _bare_host(host: str) -> str:
        host = (host or "").lower()
        return host[4:] if host.startswith("www.") else host

    # ── QWebEngineUrlRequestInterceptor override ────────────────────────
    def interceptRequest(self, info):
        if not self._enabled:
            return
        try:
            qurl = info.requestUrl()
            host = (qurl.host() or "").lower()
        except Exception:
            return
        if not host:
            return

        if self._bare_host(host) in self._allowlist_hosts:
            return

        if self._matches_blocklist(host, qurl):
            try:
                info.block(True)
            except Exception:
                return
            with self._lock:
                self._count += 1
                count = self._count
            self.blocked_count_changed.emit(count)

    def _matches_blocklist(self, host: str, qurl) -> bool:
        if host in _DOMAIN_ONLY:
            return True
        for domain in _DOMAIN_ONLY:
            if host.endswith("." + domain):
                return True
        if _DOMAIN_PATH:
            try:
                url_str = qurl.toString()
            except Exception:
                return False
            for entry in _DOMAIN_PATH:
                if entry in url_str:
                    return True
        return False
