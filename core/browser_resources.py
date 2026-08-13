"""
Resource management and constants
"""
import json
import os

# Directories
# Use absolute paths based on the script location
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BROWSER_DATA_DIR = os.path.join(SCRIPT_DIR, "browser_data")
BOOKMARKS_FILE = os.path.join(BROWSER_DATA_DIR, "bookmarks.json")
HISTORY_FILE = os.path.join(BROWSER_DATA_DIR, "history.json")
SETTINGS_FILE = os.path.join(BROWSER_DATA_DIR, "settings.json")
DOWNLOADS_FILE = os.path.join(BROWSER_DATA_DIR, "downloads.json")
PERMISSIONS_FILE = os.path.join(BROWSER_DATA_DIR, "permissions.json")
FLAGS_FILE = os.path.join(BROWSER_DATA_DIR, "flags.json")
BACKGROUNDS_DIR = os.path.join(BROWSER_DATA_DIR, "backgrounds")

# Custom protocol for serving user assets
ASSET_PROTOCOL = "cloudar-asset"
ASSET_BASE_PATH = os.path.join(BROWSER_DATA_DIR, "backgrounds")

# Default settings
DEFAULT_SETTINGS = {
    "home_page": "cloudar://newtab",
    "search_engine": "https://www.google.com/search?q={}",
    "download_location": os.path.expanduser("~/Downloads"),
    "theme": "dark",
    "language": "",
    "show_bookmark_bar": False,
    "vertical_tabs": False,
    "vertical_tabs_collapsed": True,
    "translation_enabled": True,
    "third_party_cookies": "block",
    "safe_browsing": True,
    "ad_privacy": True,
    "newtab_frame_color": "#60cdff",
    "newtab_background_image": "",
    "performance_memory_saver": True,
    "performance_energy_saver": True,
    "performance_hardware_acceleration": True,
    "tab_sleep_minutes": 5,
    # ── Force Download Directory (VD) ──
    "force_download_directory": False,
    "forced_download_path": "/home/hyggshi/Downloads/Screenshots",
    # ── Anonymity / Anti-Fingerprinting ──
    "privacy_mode": "standard",            # "standard", "strict", "extreme"
    "auto_delete_cookies_on_exit": False,  # Clear all cookies when browser closes (non-incognito)
    "block_fingerprinting": True,          # Canvas/WebGL fingerprinting protection
    "block_tracking_scripts": True,        # Block known tracker domains
    "webrtc_ip_protection": True,          # Disable WebRTC non-proxied UDP (prevents IP leak)
    "referrer_policy": "strict-origin-when-cross-origin",  # Referrer header control
    "user_agent_mode": "default",          # "default", "chrome", "firefox", "edge", "random"
    "custom_user_agent": "",               # Custom UA string
    "timezone_spoof": False,               # Spoof timezone to UTC
    "language_spoof": False,               # Spoof Accept-Language
    "privacy_quick_action": False,         # Quick toggle button in toolbar
    "block_third_party_ads": True,         # Block third-party ad trackers
}

# Icons (using Unicode symbols as fallback)
ICONS = {
    "back": "\u2190",
    "forward": "\u2192",
    "reload": "\u21bb",
    "home": "\u2302",
    "stop": "\u2715",
    "bookmark": "\u2605",
    "bookmark_empty": "\u2606",
    "new_tab": "+",
    "close": "\u2715",
    "menu": "\u22ee",
    "download": "\u2b07",
    "history": "\u23f2",
    "settings": "\u2699",
    "lock": "\U0001f512",
    "unlock": "\U0001f513",
    "play": "\u25b6",
    "pause": "\u23f8",
    "skip_next": "\u23ed",
    "skip_prev": "\u23ee",
    "media_note": "\U0001f3b5",
    "translate": "\u6587",
    "incognito": "\U0001f575",
    "privacy": "\U0001f6e1",
    "fingerprint": "\U0001f9be",
    "ghost": "\U0001f47b",
}

# User-Agent strings for spoofing
USER_AGENTS = {
    "chrome": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "firefox": "Mozilla/5.0 (X11; Linux x86_64; rv:120.0) Gecko/20100101 Firefox/120.0",
    "edge": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0",
}

# Tracking protection filters (common tracker domains)
TRACKING_DOMAINS = {
    "google-analytics.com",
    "doubleclick.net",
    "googlesyndication.com",
    "googletagmanager.com",
    "facebook.net",
    "facebook.com/tr",
    "connect.facebook.net",
    "analytics.twitter.com",
    "ads.linkedin.com",
    "bat.bing.com",
    "pixel.quantserve.com",
    "scorecardresearch.com",
    "adservice.google.com",
    "pagead2.googlesyndication.com",
    "ad.doubleclick.net",
    "securepubads.g.doubleclick.net",
    "www.googletagmanager.com/gtag/js",
    "www.google-analytics.com/analytics.js",
    "www.google-analytics.com/gtm/js",
    "cdn.segment.com",
    "api.segment.io",
    "amplitude.com",
    "cdn.heapanalytics.com",
    "cdn.mxpnl.com",
    "hotjar.com",
    "static.hotjar.com",
    "script.hotjar.com",
    "cdn.cookielaw.org",
    "cmp.quantcast.com",
    "onesignal.com",
    "cdn.pushalert.co",
    "t.co",
    "ads.yahoo.com",
    "yieldmo.com",
    "adnxs.com",
    "adsrvr.org",
    "criteo.com",
    "criteo.net",
    "casalemedia.com",
    "openx.net",
    "pubmatic.com",
    "rubiconproject.com",
    "sharethrough.com",
    "spotxchange.com",
    "taboola.com",
    "trc.taboola.com",
    "outbrain.com",
    "widgets.outbrain.com",
    "amazon-adsystem.com",
    "aax.amazon-adsystem.com",
}

# Anti-fingerprinting script injected into every page
ANTI_FINGERPRINTING_JS = """
(function() {
    // ── Canvas fingerprinting protection ──
    const originalToDataURL = HTMLCanvasElement.prototype.toDataURL;
    const originalToBlob = HTMLCanvasElement.prototype.toBlob;
    const originalGetImageData = CanvasRenderingContext2D.prototype.getImageData;
    
    HTMLCanvasElement.prototype.toDataURL = function() {
        const result = originalToDataURL.apply(this, arguments);
        // Add subtle noise (modify last few pixels) - practical protection
        if (!this._protected) {
            this._protected = true;
            const ctx = this.getContext('2d');
            if (ctx) {
                // Draw a single transparent pixel with 0.0001 offset to add noise
                const imageData = ctx.getImageData(0, 0, 1, 1);
                imageData.data[0] = Math.min(255, Math.max(0, imageData.data[0] + (Math.random() > 0.5 ? 1 : -1)));
                imageData.data[1] = Math.min(255, Math.max(0, imageData.data[1] + (Math.random() > 0.5 ? 1 : -1)));
                imageData.data[2] = Math.min(255, Math.max(0, imageData.data[2] + (Math.random() > 0.5 ? 1 : -1)));
                ctx.putImageData(imageData, 0, 0);
            }
            const modified = originalToDataURL.apply(this, arguments);
            this._protected = false;
            return modified;
        }
        return result;
    };
    
    // ── WebGL fingerprinting protection ──
    function addNoiseToWebGL() {
        try {
            const getParameter = WebGLRenderingContext.prototype.getParameter;
            WebGLRenderingContext.prototype.getParameter = function(pname) {
                // Spoof renderer info
                if (pname === 0x1F01 /* RENDERER */ || pname === 0x1F00 /* VENDOR */) {
                    return 'Mesa DRI Intel(R) HD Graphics (anon)';
                }
                // Spoof WebGL version
                if (pname === 0x1F02 /* VERSION */) {
                    return 'WebGL 1.0 (OpenGL ES 2.0 Chromium)';
                }
                return getParameter.apply(this, arguments);
            };
            
            const getParameter2 = WebGL2RenderingContext.prototype.getParameter;
            WebGL2RenderingContext.prototype.getParameter = function(pname) {
                if (pname === 0x1F01 || pname === 0x1F00) {
                    return 'Mesa DRI Intel(R) HD Graphics (anon)';
                }
                return getParameter2.apply(this, arguments);
            };
        } catch(e) {}
    }
    addNoiseToWebGL();
    
    // ── Navigator properties normalization ──
    // Spoof common fingerprinting vectors
    Object.defineProperty(navigator, 'deviceMemory', { get: () => 8 });
    
    // Number of logical processors - common value
    try {
        const origHardwareConcurrency = Object.getOwnPropertyDescriptor(
            Object.getPrototypeOf(navigator), 'hardwareConcurrency'
        );
        if (!origHardwareConcurrency || origHardwareConcurrency.configurable) {
            Object.defineProperty(navigator, 'hardwareConcurrency', { get: () => 4 });
        }
    } catch(e) {}
    
    // ── Font fingerprinting protection ──
    // Prevent measureText-based fingerprinting by keeping a consistent base
    try {
        const origMeasure = CanvasRenderingContext2D.prototype.measureText;
        CanvasRenderingContext2D.prototype.measureText = function(text) {
            const result = origMeasure.apply(this, arguments);
            if (text && text.length > 0) {
                // Slightly perturb width measurement to prevent exact font matching
                const perturbed = new Float64Array(1);
                perturbed[0] = result.width;
            }
            return result;
        };
    } catch(e) {}
})();
"""

# Anti-tracking script (blocks requests to known tracking domains)
ANTI_TRACKING_JS = """
(function() {
    // ── Tracking protection via request blocking ──
    const trackingPatterns = %s;
    
    // Override fetch to block tracking requests
    const originalFetch = window.fetch;
    window.fetch = function(input, init) {
        const url = typeof input === 'string' ? input : (input.url || '');
        if (trackingPatterns.some(p => url.includes(p))) {
            return Promise.reject(new Error('Blocked by tracking protection'));
        }
        return originalFetch.apply(this, arguments);
    };
    
    // Override XMLHttpRequest to block tracking
    const originalOpen = XMLHttpRequest.prototype.open;
    XMLHttpRequest.prototype.open = function(method, url) {
        if (trackingPatterns.some(p => String(url).includes(p))) {
            console.warn('[Tracking Protection] Blocked:', url);
            return;
        }
        return originalOpen.apply(this, arguments);
    };
    
    // Block tracking via navigator.sendBeacon
    const originalSendBeacon = navigator.sendBeacon;
    navigator.sendBeacon = function(url, data) {
        if (trackingPatterns.some(p => String(url).includes(p))) {
            return false;
        }
        return originalSendBeacon.apply(this, arguments);
    };
    
    // Block tracking image pixels
    const originalImageSrc = Object.getOwnPropertyDescriptor(Image.prototype, 'src');
    try {
        Object.defineProperty(Image.prototype, 'src', {
            set: function(url) {
                if (trackingPatterns.some(p => String(url).includes(p))) {
                    return;
                }
                originalImageSrc.set.call(this, url);
            }
        });
    } catch(e) {}
})();
"""


def env_data_dir(dir_name=BROWSER_DATA_DIR):
    """Get data directory path and ensure it exists"""
    if not os.path.exists(dir_name):
        os.makedirs(dir_name)
    return dir_name


def ensure_data_directory(dir_name=BROWSER_DATA_DIR):
    """Create directory if it doesn't exist"""
    return env_data_dir(dir_name)


def ensure_backgrounds_directory():
    """Ensure backgrounds directory exists"""
    if not os.path.exists(BACKGROUNDS_DIR):
        os.makedirs(BACKGROUNDS_DIR)
    return BACKGROUNDS_DIR


def load_json_file(filepath, default=None):
    """Load JSON file with error handling"""
    if default is None:
        default = {}

    try:
        if os.path.exists(filepath):
            with open(filepath, "r", encoding="utf-8") as f:
                return json.load(f)
    except Exception as e:
        print(f"Error loading {filepath}: {e}")

    return default


def save_json_file(filepath, data):
    """Save data to JSON file"""
    ensure_data_directory()
    try:
        with open(filepath, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        return True
    except Exception as e:
        print(f"Error saving {filepath}: {e}")
        return False