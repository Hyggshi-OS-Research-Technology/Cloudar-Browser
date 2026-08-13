import json
import os
import uuid
from core.browser_qt import QObject, pyqtSignal, pyqtSlot
from core.browser_resources import (SETTINGS_FILE, DEFAULT_SETTINGS, load_json_file, save_json_file,
                              BACKGROUNDS_DIR, ensure_backgrounds_directory)
from features.language_manager import LanguageManager
from core.language import get_available_languages as core_get_available_languages


class SettingsBackend(QObject):
    """Bridge between web-based settings UI and Python settings storage."""

    settingsChanged = pyqtSignal(dict)

    def __init__(self, browser_window):
        super().__init__(browser_window)
        self.browser = browser_window
        root_dir = os.path.dirname(os.path.abspath(__file__))
        self.config_dir = os.path.join(root_dir, "config")
        self.config_path = os.path.join(self.config_dir, "settings.json")
        self.settings = self._load_settings()

    def _migrate_base64_background(self, bg_value):
        """
        Migrate old base64 background images to file-based storage.
        Returns cloudar-asset:// URL if migration successful, original value otherwise.
        """
        if not bg_value or not bg_value.startswith('data:'):
            return bg_value
        
        try:
            import base64
            import mimetypes
            
            # Parse data URL
            header, data = bg_value.split(',', 1)
            mime_type = header.split(';')[0].split(':')[1]
            ext = mimetypes.guess_extension(mime_type) or '.jpg'
            
            # Decode base64
            image_data = base64.b64decode(data)
            
            # Save to file
            ensure_backgrounds_directory()
            unique_name = f"bg_{uuid.uuid4().hex[:12]}{ext}"
            dest_path = os.path.join(BACKGROUNDS_DIR, unique_name)
            
            with open(dest_path, 'wb') as f:
                f.write(image_data)
            
            # Resize if needed
            self._resize_image(dest_path, max_width=1920, max_height=1080)
            
            # Create cloudar-asset:// URL (secure custom protocol)
            asset_url = f"cloudar-asset://{unique_name}"
            
            print(f"Migrated base64 background to file: {unique_name}")
            return asset_url
        except Exception as e:
            print(f"Error migrating base64 background: {e}")
            return bg_value

    def _load_settings(self):
        os.makedirs(self.config_dir, exist_ok=True)

        if os.path.exists(self.config_path):
            try:
                with open(self.config_path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                if "tab_sleep_minutes" not in data:
                    data["tab_sleep_minutes"] = 5
                
                # Migrate base64 background to file-based storage
                if "background" in data and data["background"]:
                    data["background"] = self._migrate_base64_background(data["background"])
                
                return data
            except Exception:
                pass

        # Build defaults from browser settings
        browser_settings = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)
        data = self._from_browser_settings(browser_settings)
        self._save_config(data)
        return data

    def _save_config(self, data):
        os.makedirs(self.config_dir, exist_ok=True)
        with open(self.config_path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)

    def _from_browser_settings(self, browser_settings):
        startup = browser_settings.get("startup_behavior", "Open New Tab")
        if startup == "Continue where you left off":
            startup_key = "continue"
        elif startup == "Open Home Page":
            startup_key = "homepage"
        else:
            startup_key = "newtab"

        theme = browser_settings.get("theme", "Dark").lower()

        return {
            "homepage": browser_settings.get("home_page", "cloudar://newtab"),
            "search_engine": browser_settings.get("search_engine", "https://www.google.com/search?q={}"),
            "theme": theme if theme in ("dark", "light", "system") else "dark",
            "accent_color": browser_settings.get("newtab_frame_color", "#60cdff"),
            "background": browser_settings.get("newtab_background_image", ""),
            "startup": startup_key,
            "downloads_folder": browser_settings.get("download_location", ""),
            "ask_before_download": browser_settings.get("ask_download", False),
            "hardware_accel": browser_settings.get("performance_hardware_acceleration", True),
            "incognito": browser_settings.get("incognito_enabled", True),
            "language": browser_settings.get("language", ""),
            "vertical_tabs": browser_settings.get("vertical_tabs", False),
            "vertical_tabs_collapsed": browser_settings.get("vertical_tabs_collapsed", True),
            "tab_sleep_minutes": browser_settings.get("tab_sleep_minutes", 5),
            "version": "1.0.0",
            "engine": "Qt WebEngine",
            # ── Anonymity settings ──
            "privacy_mode": browser_settings.get("privacy_mode", "standard"),
            "auto_delete_cookies_on_exit": browser_settings.get("auto_delete_cookies_on_exit", False),
            "block_fingerprinting": browser_settings.get("block_fingerprinting", True),
            "block_tracking_scripts": browser_settings.get("block_tracking_scripts", True),
            "webrtc_ip_protection": browser_settings.get("webrtc_ip_protection", True),
            "user_agent_mode": browser_settings.get("user_agent_mode", "default"),
            "custom_user_agent": browser_settings.get("custom_user_agent", ""),
            "block_third_party_ads": browser_settings.get("block_third_party_ads", True),
        }

    def _apply_to_browser(self, web_settings):
        current = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)

        if "homepage" in web_settings:
            current["home_page"] = web_settings["homepage"] or "cloudar://newtab"

        if "search_engine" in web_settings:
            current["search_engine"] = web_settings["search_engine"]

        if "startup" in web_settings:
            if web_settings["startup"] == "continue":
                current["startup_behavior"] = "Continue where you left off"
            elif web_settings["startup"] == "homepage":
                current["startup_behavior"] = "Open Home Page"
            else:
                current["startup_behavior"] = "Open New Tab"

        if "theme" in web_settings:
            theme = web_settings["theme"]
            if theme == "dark":
                current["theme"] = "Dark"
            elif theme == "light":
                current["theme"] = "Light"
            else:
                current["theme"] = current.get("theme", "Dark")
        
        if "accent_color" in web_settings:
            current["newtab_frame_color"] = web_settings["accent_color"]
            
        if "background" in web_settings:
            current["newtab_background_image"] = web_settings["background"]

        if "downloads_folder" in web_settings:
            current["download_location"] = web_settings["downloads_folder"]

        if "ask_before_download" in web_settings:
            current["ask_download"] = bool(web_settings["ask_before_download"])

        if "hardware_accel" in web_settings:
            current["performance_hardware_acceleration"] = bool(web_settings["hardware_accel"])

        if "incognito" in web_settings:
            current["incognito_enabled"] = bool(web_settings["incognito"])

        if "vertical_tabs" in web_settings:
            current["vertical_tabs"] = bool(web_settings["vertical_tabs"])

        if "vertical_tabs_collapsed" in web_settings:
            current["vertical_tabs_collapsed"] = bool(web_settings["vertical_tabs_collapsed"])

        if "language" in web_settings:
            current["language"] = web_settings["language"]
        if "tab_sleep_minutes" in web_settings:
            try:
                current["tab_sleep_minutes"] = int(web_settings["tab_sleep_minutes"])
            except Exception:
                current["tab_sleep_minutes"] = 5

        # ── Anonymity settings ──
        if "privacy_mode" in web_settings:
            current["privacy_mode"] = web_settings["privacy_mode"]
        if "auto_delete_cookies_on_exit" in web_settings:
            current["auto_delete_cookies_on_exit"] = bool(web_settings["auto_delete_cookies_on_exit"])
        if "block_fingerprinting" in web_settings:
            current["block_fingerprinting"] = bool(web_settings["block_fingerprinting"])
        if "block_tracking_scripts" in web_settings:
            current["block_tracking_scripts"] = bool(web_settings["block_tracking_scripts"])
        if "webrtc_ip_protection" in web_settings:
            current["webrtc_ip_protection"] = bool(web_settings["webrtc_ip_protection"])
        if "user_agent_mode" in web_settings:
            current["user_agent_mode"] = web_settings["user_agent_mode"]
        if "custom_user_agent" in web_settings:
            current["custom_user_agent"] = web_settings["custom_user_agent"]
        if "block_third_party_ads" in web_settings:
            current["block_third_party_ads"] = bool(web_settings["block_third_party_ads"])

        save_json_file(SETTINGS_FILE, current)
        self.browser.on_settings_changed(current)

    def apply_loaded_settings(self):
        """Apply settings loaded from config/settings.json to the browser."""
        if not isinstance(self.settings, dict):
            return

        # Load current browser settings file
        current = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)

        # Map config keys to browser settings keys, always applying homepage
        if "homepage" in self.settings:
            current["home_page"] = self.settings["homepage"]
        if "search_engine" in self.settings:
            current["search_engine"] = self.settings["search_engine"]
        if "startup" in self.settings:
            startup_map = {
                "continue": "Continue where you left off",
                "homepage": "Open Home Page",
                "newtab": "Open New Tab",
            }
            current["startup_behavior"] = startup_map.get(self.settings["startup"], "Open New Tab")
        if "theme" in self.settings:
            theme = self.settings["theme"]
            if theme in ("dark", "light"):
                current["theme"] = theme.capitalize()
        
        if "accent_color" in self.settings:
            current["newtab_frame_color"] = self.settings["accent_color"]
        if "background" in self.settings:
            current["newtab_background_image"] = self.settings["background"]

        if "downloads_folder" in self.settings:
            current["download_location"] = self.settings["downloads_folder"]
        if "ask_before_download" in self.settings:
            current["ask_download"] = bool(self.settings["ask_before_download"])
        if "hardware_accel" in self.settings:
            current["performance_hardware_acceleration"] = bool(self.settings["hardware_accel"])
        if "incognito" in self.settings:
            current["incognito_enabled"] = bool(self.settings["incognito"])
        if "vertical_tabs" in self.settings:
            current["vertical_tabs"] = bool(self.settings["vertical_tabs"])
        if "vertical_tabs_collapsed" in self.settings:
            current["vertical_tabs_collapsed"] = bool(self.settings["vertical_tabs_collapsed"])
        if "language" in self.settings:
            current["language"] = self.settings["language"]
        if "tab_sleep_minutes" in self.settings:
            try:
                current["tab_sleep_minutes"] = int(self.settings["tab_sleep_minutes"])
            except Exception:
                current["tab_sleep_minutes"] = 5

        # ── Anonymity settings ──
        if "privacy_mode" in self.settings:
            current["privacy_mode"] = self.settings["privacy_mode"]
        if "auto_delete_cookies_on_exit" in self.settings:
            current["auto_delete_cookies_on_exit"] = bool(self.settings["auto_delete_cookies_on_exit"])
        if "block_fingerprinting" in self.settings:
            current["block_fingerprinting"] = bool(self.settings["block_fingerprinting"])
        if "block_tracking_scripts" in self.settings:
            current["block_tracking_scripts"] = bool(self.settings["block_tracking_scripts"])
        if "webrtc_ip_protection" in self.settings:
            current["webrtc_ip_protection"] = bool(self.settings["webrtc_ip_protection"])
        if "user_agent_mode" in self.settings:
            current["user_agent_mode"] = self.settings["user_agent_mode"]
        if "custom_user_agent" in self.settings:
            current["custom_user_agent"] = self.settings["custom_user_agent"]
        if "block_third_party_ads" in self.settings:
            current["block_third_party_ads"] = bool(self.settings["block_third_party_ads"])

        save_json_file(SETTINGS_FILE, current)
        self.browser.on_settings_changed(current)

    @pyqtSlot(result='QVariant')
    def getSettings(self):
        return self.settings

    @pyqtSlot('QVariant')
    def updateSettings(self, data):
        if isinstance(data, dict):
            self.settings.update(data)
            self._save_config(self.settings)
            self._apply_to_browser(data)
            self.settingsChanged.emit(self.settings)

    @pyqtSlot(str)
    def saveNewTabSettings(self, settings_json):
        """Specifically for the newtab page customization."""
        try:
            settings_data = json.loads(settings_json)
            color = settings_data.get('accentColor')
            bg = settings_data.get('bgImage')
            
            # Also update the browser's main settings which are shared
            current = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)
            if color:
                current["newtab_frame_color"] = color
                self.settings["accent_color"] = color
            if bg is not None:
                current["newtab_background_image"] = bg
                self.settings["background"] = bg
                
            save_json_file(SETTINGS_FILE, current)
            self.browser.settings = current
            self._save_config(self.settings)
            
            self.settingsChanged.emit(self.settings)
        except Exception as e:
            print(f"Error saving newtab settings: {e}")

    def _cleanup_old_backgrounds(self, keep_file=None):
        """
        Remove old background images to prevent disk bloat.
        Keeps the currently active background (keep_file) and removes others.
        """
        try:
            if not os.path.exists(BACKGROUNDS_DIR):
                return
            
            # Get all background files
            bg_files = [f for f in os.listdir(BACKGROUNDS_DIR) 
                       if f.startswith('bg_') and os.path.isfile(os.path.join(BACKGROUNDS_DIR, f))]
            
            # Remove old backgrounds (keep the current one and last 5 for undo/history)
            if len(bg_files) > 6:  # Keep current + 5 previous
                # Sort by modification time (oldest first)
                bg_files.sort(key=lambda f: os.path.getmtime(os.path.join(BACKGROUNDS_DIR, f)))
                
                # Remove oldest files, keeping the 6 most recent
                files_to_remove = bg_files[:-6]
                for old_file in files_to_remove:
                    old_path = os.path.join(BACKGROUNDS_DIR, old_file)
                    try:
                        os.remove(old_path)
                        print(f"Cleaned up old background: {old_file}")
                    except Exception as e:
                        print(f"Error removing old background {old_file}: {e}")
        except Exception as e:
            print(f"Error during background cleanup: {e}")

    def _resize_image(self, image_path, max_width=1920, max_height=1080):
        """
        Resize image to fit within max_width x max_height while maintaining aspect ratio.
        Returns path to resized image (or original if no resize needed).
        """
        try:
            from PIL import Image
            
            with Image.open(image_path) as img:
                # Convert to RGB if necessary (for PNG with alpha, etc.)
                if img.mode in ('RGBA', 'LA', 'P'):
                    # Keep alpha channel for PNG
                    if img.mode != 'RGBA':
                        img = img.convert('RGBA')
                elif img.mode != 'RGB':
                    img = img.convert('RGB')
                
                # Calculate new size maintaining aspect ratio
                original_width, original_height = img.size
                
                # Only resize if image is larger than target
                if original_width > max_width or original_height > max_height:
                    ratio = min(max_width / original_width, max_height / original_height)
                    new_width = int(original_width * ratio)
                    new_height = int(original_height * ratio)
                    
                    # Use high-quality resampling
                    img = img.resize((new_width, new_height), Image.Resampling.LANCZOS)
                    
                    # Save resized image
                    img.save(image_path, quality=90, optimize=True)
                    print(f"Resized background image from {original_width}x{original_height} to {new_width}x{new_height}")
                
                return image_path
        except ImportError:
            print("Pillow not installed, skipping image resize")
            return image_path
        except Exception as e:
            print(f"Error resizing image: {e}")
            return image_path

    @pyqtSlot(result='QString')
    def selectBgImage(self):
        """
        Open a file dialog to pick a local image for the new tab background.
        Optimized: saves to file instead of base64, resizes to display size.
        """
        from core.browser_qt import QFileDialog
        import base64, mimetypes, shutil
        file_path, _ = QFileDialog.getOpenFileName(
            self.browser,
            "Select Background Image",
            "",
            "Images (*.png *.jpg *.jpeg *.webp *.gif *.bmp *.svg)"
        )
        if file_path:
            try:
                # Ensure backgrounds directory exists
                ensure_backgrounds_directory()
                
                # Generate unique filename
                ext = os.path.splitext(file_path)[1].lower()
                if not ext:
                    ext = '.jpg'
                unique_name = f"bg_{uuid.uuid4().hex[:12]}{ext}"
                dest_path = os.path.join(BACKGROUNDS_DIR, unique_name)
                
                # Copy and resize the image
                shutil.copy2(file_path, dest_path)
                self._resize_image(dest_path, max_width=1920, max_height=1080)
                
                # Create cloudar-asset:// URL (secure custom protocol)
                asset_url = f"cloudar-asset://{unique_name}"
                
                # Persist
                current = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)
                current["newtab_background_image"] = asset_url
                self.settings["background"] = asset_url
                save_json_file(SETTINGS_FILE, current)
                self.browser.settings = current
                self._save_config(self.settings)
                
                # Clean up old backgrounds to prevent disk bloat
                self._cleanup_old_backgrounds(keep_file=unique_name)
                
                self.settingsChanged.emit(self.settings)
                return asset_url
            except Exception as e:
                print(f"Error reading background image: {e}")
        return ""

    @pyqtSlot(result='QString')
    def selectDownloadFolder(self):
        from core.browser_qt import QFileDialog
        folder = QFileDialog.getExistingDirectory(self.browser, "Select Download Location")
        if folder:
            self.settings["downloads_folder"] = folder
            self._save_config(self.settings)
            self._apply_to_browser({"downloads_folder": folder})
            self.settingsChanged.emit(self.settings)
            return folder
        return ""

    @pyqtSlot()
    def clearBrowsingData(self):
        # Delegate to BrowserWindow data clearing
        self.browser.on_settings_changed({"action": "clear_data"})

    # ── Language methods ──────────────────────────────────────────

    @pyqtSlot(result='QVariant')
    def getAvailableLanguages(self):
        """Return dict of {code: native_name} for all available languages."""
        lang = LanguageManager.instance()
        return lang.get_available_languages()

    @pyqtSlot(result='QString')
    def getCurrentLanguage(self):
        """Return the current language code."""
        lang = LanguageManager.instance()
        return lang.get_current_language()

    @pyqtSlot(str)
    def setLanguage(self, code):
        """
        Switch the browser language at runtime.
        Persists the choice and triggers UI retranslation.
        """
        lang = LanguageManager.instance()

        # Persist to both config stores
        self.settings["language"] = code
        self._save_config(self.settings)

        current = load_json_file(SETTINGS_FILE, DEFAULT_SETTINGS)
        current["language"] = code
        save_json_file(SETTINGS_FILE, current)

        # Keep browser_window.settings in sync
        self.browser.settings = current

        # Switch language (emits language_changed signal -> retranslate_ui)
        lang.switch_language(code)

    @pyqtSlot(result='QVariant')
    def getLanguageInfo(self):
        """
        Return language info for the settings page.
        Returns {current: "en", available: {"en": "English", ...}}
        """
        lang = LanguageManager.instance()
        return {
            "current": lang.get_current_language(),
            "available": core_get_available_languages()
        }