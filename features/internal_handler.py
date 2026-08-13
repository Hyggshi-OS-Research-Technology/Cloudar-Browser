from core.browser_qt import QWebEngineUrlSchemeHandler, QUrl, QByteArray, QBuffer, QIODevice, QWebEngineUrlRequestJob
import os
import json
from urllib.parse import parse_qs
from core.browser_resources import BACKGROUNDS_DIR
from core.language import get_text as tr

class AssetSchemeHandler(QWebEngineUrlSchemeHandler):
    """Handles cloudar-asset:// URL scheme to serve user assets (backgrounds, etc.)"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.browser = parent
        # Use absolute path and ensure it exists
        self.asset_base_path = os.path.abspath(BACKGROUNDS_DIR)
        # Debug: print the path on initialization
        print(f"AssetSchemeHandler initialized with path: {self.asset_base_path}")
        print(f"Path exists: {os.path.exists(self.asset_base_path)}")
    
    def requestStarted(self, job):
        """
        Serve files from browser_data/backgrounds/ via cloudar-asset:// protocol
        Example: cloudar-asset://bg_abc123.jpg -> browser_data/backgrounds/bg_abc123.jpg
        """
        request_url = job.requestUrl()
        host = request_url.host()  # e.g. 'bg_abc123.jpg'
        path_str = request_url.path()
        
        print(f"Asset request: host={host}, path={path_str}, base_path={self.asset_base_path}")
        
        if not host:
            print("Asset request failed: no host")
            job.fail(QWebEngineUrlRequestJob.Error.UrlNotFound)
            return
        
        # Construct file path - host is the filename
        filename = host
        file_path = os.path.normpath(os.path.join(self.asset_base_path, filename))
        
        print(f"Looking for file: {file_path}")
        print(f"File exists: {os.path.exists(file_path)}")
        
        # Security: ensure path is within backgrounds directory
        if not file_path.startswith(os.path.normpath(self.asset_base_path)):
            print(f"Asset path traversal attempt blocked: {filename}")
            job.fail(QWebEngineUrlRequestJob.Error.UrlInvalid)
            return
        
        if not os.path.exists(file_path) or not os.path.isfile(file_path):
            print(f"Asset not found: {file_path}")
            job.fail(QWebEngineUrlRequestJob.Error.UrlNotFound)
            return
        
        try:
            with open(file_path, 'rb') as f:
                data = f.read()
            
            # Determine MIME type from extension
            ext = os.path.splitext(filename)[1].lower()
            mime_map = {
                '.jpg': b'image/jpeg',
                '.jpeg': b'image/jpeg',
                '.png': b'image/png',
                '.webp': b'image/webp',
                '.gif': b'image/gif',
                '.svg': b'image/svg+xml',
                '.bmp': b'image/bmp',
            }
            mime = mime_map.get(ext, b'application/octet-stream')
            
            buffer = QBuffer(job)
            buffer.setData(data)
            job.reply(mime, buffer)
        except Exception as e:
            print(f"Error serving asset {filename}: {e}")
            job.fail(QWebEngineUrlRequestJob.Error.RequestFailed)

class InternalSchemeHandler(QWebEngineUrlSchemeHandler):
    """Handles cloudar:// URL scheme to serve internal pages"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.browser = parent
        root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.pages_dir = os.path.join(root_dir, "internal_pages")
        self.resources_dir = os.path.join(root_dir, "resources")
        self.page_roots = {
            "settings": os.path.join(self.resources_dir, "pages", "settings"),
            "newtab": self.pages_dir,
            "about": self.pages_dir,
            "bookmarks": self.pages_dir,
            "history": self.pages_dir,
            "downloads": self.pages_dir,
            "permissions": self.pages_dir,
            "extensions": self.pages_dir,
            "flags": self.pages_dir
        }

    def _resolve_page_path(self, host, path_str):
        if host not in self.page_roots:
            return None

        base_dir = self.page_roots[host]
        if not base_dir:
            return None

        if host == "settings":
            rel_path = "index.html" if path_str in ("", "/") else path_str.lstrip("/")
        elif host in ("newtab", "about", "bookmarks", "history", "downloads", "permissions", "extensions", "flags"):
            rel_path = f"{host}.html" if path_str in ("", "/") else path_str.lstrip("/")
        else:
            rel_path = path_str.lstrip("/")

        full_path = os.path.normpath(os.path.join(base_dir, rel_path))
        if not full_path.startswith(os.path.normpath(base_dir)):
            return None
        return full_path

    def requestStarted(self, job):
        """
        Processes the request for cloudar://
        job is a QWebEngineUrlRequestJob
        """
        request_url = job.requestUrl()
        host = request_url.host()  # e.g. 'newtab' in cloudar://newtab
        path_str = request_url.path()
        if not host and path_str in ("", "/"):
            host = "newtab"
        elif host == "newt" and path_str.lstrip("/") == "ab":
            host = "newtab"
            path_str = ""
        elif host == "tab":
            host = "newtab"
            path_str = ""
        fragment = request_url.fragment()  # e.g. 'language' in cloudar://settings#language

        # Page Serving
        path = self._resolve_page_path(host, path_str)

        if path and os.path.exists(path):
            try:
                with open(path, 'rb') as f:
                    data = f.read()

                # Setup Mime Type
                mime = b"text/html"
                if path.endswith(".css"):
                    mime = b"text/css"
                elif path.endswith(".js"):
                    mime = b"application/javascript"
                elif path.endswith(".json"):
                    mime = b"application/json"
                elif path.endswith(".svg"):
                    mime = b"image/svg+xml"
                elif path.endswith(".png"):
                    mime = b"image/png"

                # ── Inject Settings into newtab.html ────────────────
                if host == "newtab" and self.browser:
                    settings = self.browser.settings
                    settings_js = {
                        "accentColor": settings.get("newtab_frame_color", "#60cdff"),
                        "bgImage": settings.get("newtab_background_image", "")
                    }

                    injection = f"""
                    <script>
                        window.INJECTED_SETTINGS = {json.dumps(settings_js)};
                    </script>
                    """.encode('utf-8')

                    if b"</head>" in data:
                        data = data.replace(b"</head>", injection + b"</head>")
                    else:
                        data = injection + data

                # ── Inject fragment for settings page routing ───────
                # QtWebEngine doesn't always preserve the fragment (#section)
                # for custom URL schemes, so we inject it into the page
                # so JavaScript can read it via window.INJECTED_FRAGMENT.
                if host == "settings" and fragment:
                    fragment_json = json.dumps(fragment)
                    fragment_injection = f"""
                    <script>
                        window.INJECTED_FRAGMENT = {fragment_json};
                    </script>
                    """.encode('utf-8')

                    if b"</head>" in data:
                        data = data.replace(b"</head>", fragment_injection + b"</head>")
                    else:
                        data = fragment_injection + data

                # ── Inject translated static strings for cloudar://flags ──
                if host == "flags":
                    flags_strings = {
                        k: tr(k) for k in (
                            "flags_reset_all", "flags_experimental_warning",
                            "flags_search_placeholder", "flags_relaunch_banner",
                            "flags_relaunch_button", "flags_status_enabled",
                            "flags_status_disabled", "flags_status_default",
                            "flags_no_results",
                        )
                    }
                    strings_injection = f"""
                    <script>
                        window.INJECTED_STRINGS = {json.dumps(flags_strings)};
                    </script>
                    """.encode('utf-8')

                    if b"</head>" in data:
                        data = data.replace(b"</head>", strings_injection + b"</head>")
                    else:
                        data = strings_injection + data

                buffer = QBuffer(job)
                buffer.setData(data)
                job.reply(mime, buffer)
            except Exception as e:
                print(f"Error serving internal page {host}: {e}")
                job.fail(QWebEngineUrlRequestJob.Error.RequestFailed)
        else:
            print(f"Internal page not found: {host}")
            job.fail(QWebEngineUrlRequestJob.Error.UrlNotFound)
