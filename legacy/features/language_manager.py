"""
Language Manager for Cloudar Browser™ (i18n / Internationalization)

Architecture Overview:
=====================
The language system uses JSON translation files stored in the /lang directory.
Each language has its own file (en.json, vi.json, ja.json, zh.json) containing
key-value pairs for every translatable UI string.

LanguageManager is a singleton-pattern class that:
  1. Detects the system locale on first load
  2. Falls back to English if the system language is unavailable
  3. Provides a tr(key) method to retrieve translated strings
  4. Supports runtime language switching without application restart
  5. Notifies registered widgets when the language changes via signal

Usage:
------
    from features.language_manager import LanguageManager

    lang = LanguageManager.instance()       # get singleton
    text = lang.tr("menu_file")             # returns "File" or translated text
    lang.switch_language("vi")              # switch to Vietnamese at runtime

Integration with PyQt6:
-----------------------
LanguageManager emits a `language_changed` signal. Any widget that displays
translatable text should connect to this signal and call a refresh method
to re-read all displayed strings from the manager.

File Structure:
---------------
/lang/
    en.json   - English (fallback)
    vi.json   - Vietnamese
    ja.json   - Japanese
    zh.json   - Chinese (Simplified)
"""

import json
import os
import locale
from core.browser_qt import QObject, pyqtSignal
from core.language import get_text as core_get_text, set_language as core_set_language, get_available_languages as core_get_available_languages, get_current_language as core_get_current_language


# Maps system locale prefixes to our supported language codes
LOCALE_MAP = {
    "en": "en",
    "vi": "vi",
    "ko": "ko",
    "fr": "fr",
    "de": "de",
    "es": "es",
    "ru": "ru",
    "pt": "pt",
    "pt_br": "pt",
    "pt_pt": "pt",
    "ja": "ja",
    "zh": "zh",
    "zh_cn": "zh",
    "zh_tw": "zh",
    "zh_hans": "zh",
    "zh_hant": "zh",
}


class LanguageManager(QObject):
    """
    Manages multi-language support for the browser UI.

    This class loads translation strings from JSON files and provides
    a simple tr(key) method to get translated text. It supports
    runtime language switching without restart.

    Signals:
        language_changed(str): Emitted when the active language changes.
                               The argument is the new language code (e.g., "en").
    """

    # Signal emitted when language changes â€” all connected widgets should refresh
    language_changed = pyqtSignal(str)

    _instance = None  # Singleton instance

    @classmethod
    def instance(cls):
        """Get the singleton LanguageManager instance."""
        if cls._instance is None:
            cls._instance = LanguageManager()
        return cls._instance

    @classmethod
    def reset(cls):
        """Reset the singleton (useful for testing)."""
        cls._instance = None

    def __init__(self):
        super().__init__()

        # Directory where language JSON files are stored
        self._lang_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "lang")

        # Current language code
        self._current_language = "en"

        # Available languages {code: native_name}
        self._available_languages: dict = dict(core_get_available_languages())

        # Discover available languages and load the appropriate one
        self._discover_languages()
        self._load_fallback()
        self._detect_and_load()

    @staticmethod
    def _log(message: str):
        """
        Log a message safely, handling encoding issues on Windows console.
        Uses sys.stderr as fallback when stdout encoding fails.
        """
        import sys
        try:
            print(f"[LanguageManager] {message}")
        except UnicodeEncodeError:
            # Windows console may not support all Unicode characters
            try:
                sys.stderr.write(f"[LanguageManager] {message.encode('ascii', 'replace').decode('ascii')}\n")
            except Exception:
                pass

    def _discover_languages(self):
        """
        Scan the /lang directory for available .json files.
        Populates self._available_languages with {code: native_name}.
        """
        self._available_languages = dict(core_get_available_languages())

    def _load_fallback(self):
        """Load English translations as fallback."""
        en_path = os.path.join(self._lang_dir, "en.json")
        try:
            with open(en_path, "r", encoding="utf-8") as f:
                self._fallback = json.load(f)
        except Exception as e:
            self._log(f"Error loading fallback (en.json): {e}")
            self._fallback = {}

    def _detect_and_load(self):
        """
        Detect the system locale and load the matching language.
        Falls back to English if the detected language is not available.
        """
        # Set English as the default language
        self.set_language("en")

    def _detect_system_language(self) -> str:
        """
        Detect the system language using locale.getdefaultlocale().

        Returns:
            Language code (e.g., "en", "vi", "ja", "zh") or "en" as fallback.
        """
        try:
            system_locale = locale.getdefaultlocale()[0]
            if system_locale:
                # Extract language part (e.g., "en_US" -> "en")
                lang_prefix = system_locale.lower().split("_")[0]

                # Check for zh_CN, zh_TW etc.
                if lang_prefix == "zh":
                    full_lower = system_locale.lower()
                    if full_lower in ("zh_cn", "zh_hans"):
                        return "zh" if "zh" in self._available_languages else "en"
                    elif full_lower in ("zh_tw", "zh_hant", "zh_hk"):
                        return "zh" if "zh" in self._available_languages else "en"
                    return "zh" if "zh" in self._available_languages else "en"

                if lang_prefix in self._available_languages:
                    return lang_prefix
        except Exception as e:
            self._log(f"Error detecting system locale: {e}")

        return "en"

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def tr(self, key: str, **kwargs) -> str:
        """
        Get the translated string for the given key.

        Args:
            key: The translation key (e.g., "menu_file").
            **kwargs: Optional format parameters (e.g., progress=50).

        Returns:
            The translated string. Falls back to English, then to the key itself.

        Example:
            lang.tr("loading", progress=50)  -> "Loading... 50%"
        """
        text = core_get_text(key)
        if kwargs:
            try:
                text = text.format(**kwargs)
            except (KeyError, IndexError):
                pass
        return text

    def set_language(self, code: str) -> bool:
        """
        Load and activate a language by code.

        Args:
            code: Language code (e.g., "en", "vi", "ja", "zh").

        Returns:
            True if the language was loaded successfully, False otherwise.
        """
        if code not in self._available_languages:
            self._log(f"Language '{code}' not available, falling back to 'en'")
            code = "en"

        core_set_language(code)
        self._current_language = code
        self._log(f"Loaded language: {code}")
        return True

    def switch_language(self, code: str):
        """
        Switch language at runtime and notify all connected widgets.

        This is the main method for runtime language switching.
        After calling this, all widgets connected to language_changed
        should update their displayed text.

        Args:
            code: Language code to switch to.
        """
        if self.set_language(code):
            self.language_changed.emit(code)

    def get_current_language(self) -> str:
        """Get the current language code."""
        return core_get_current_language()

    def get_available_languages(self) -> dict:
        """
        Get all available languages.

        Returns:
            Dict of {code: native_name}, e.g., {"en": "English", "vi": "Tiáº¿ng Viá»‡t"}
        """
        return dict(self._available_languages)

    def get_language_native_name(self, code: str) -> str:
        """Get the native name of a language."""
        return self._available_languages.get(code, code)

