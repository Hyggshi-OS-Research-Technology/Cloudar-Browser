"""
Chrome-extension-style match pattern matching.

Extensions declare which pages they run on via a "matches" list in their
manifest.json, e.g.:

    "matches": ["*://*.youtube.com/*", "*://youtu.be/*"]

instead of the browser having a hardcoded if/else per known extension.
Adding a new extension that needs its own bridge/behavior on specific
sites is then just a manifest change, no core/browser_window.py edit.

Pattern format: "<scheme>://<host>/<path>"
    scheme : "*" matches any scheme, otherwise must match exactly
    host   : "*" alone matches any host; a leading "*." matches the
             domain and any subdomain (e.g. "*.youtube.com" matches
             both "youtube.com" and "m.youtube.com"); "*" elsewhere in
             the host is a normal wildcard
    path   : "*" is a normal wildcard; missing path defaults to "/*"
"""
import re
from functools import lru_cache


@lru_cache(maxsize=256)
def _compile_pattern(pattern):
    try:
        scheme_part, rest = pattern.split("://", 1)
    except ValueError:
        # Not a well-formed match pattern; never match anything.
        return None

    if "/" in rest:
        host_part, path_part = rest.split("/", 1)
        path_part = "/" + path_part
    else:
        host_part, path_part = rest, "/*"

    def wildcard_to_regex(segment):
        return re.escape(segment).replace(r"\*", ".*")

    scheme_re = "[^:]+" if scheme_part == "*" else wildcard_to_regex(scheme_part)

    if host_part == "*":
        host_re = ".*"
    elif host_part.startswith("*."):
        # "*.example.com" -> "example.com" or "anything.example.com"
        host_re = "(?:.*\\.)?" + re.escape(host_part[2:]).replace(r"\*", ".*")
    else:
        host_re = wildcard_to_regex(host_part)

    path_re = wildcard_to_regex(path_part)

    try:
        return re.compile(f"^{scheme_re}://{host_re}{path_re}$", re.IGNORECASE)
    except re.error:
        return None


def url_matches(qurl, pattern):
    """Return True if a QUrl matches a single Chrome-style match pattern."""
    regex = _compile_pattern(pattern)
    if regex is None:
        return False
    host = (qurl.host() or "").lower()
    path = qurl.path() or "/"
    candidate = f"{qurl.scheme()}://{host}{path}"
    return bool(regex.match(candidate))


def url_matches_any(qurl, patterns):
    """Return True if a QUrl matches any pattern in `patterns`."""
    return any(url_matches(qurl, p) for p in (patterns or []))


def specificity(pattern):
    """Rough specificity score so a specific match (e.g. *.youtube.com)
    is preferred over a catch-all one (e.g. *://*/*) when more than one
    extension's patterns match the same URL. More literal (non-wildcard)
    characters == higher score, since that means less of the URL is
    actually left free to vary."""
    if not pattern:
        return -1
    return len(pattern.replace("*", ""))
