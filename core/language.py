"""
Core language loader for Cloudar Browser™.
"""
import json
import os

# Supported languages for UI
SUPPORTED_LANGUAGES = {
    "en": "English",
    "vi": "Vietnamese",
    "ko": "한국어",
    "fr": "Français",
    "de": "Deutsch",
    "es": "Español",
    "ru": "Русский",
    "pt": "Português",
    "ja": "Japanese",
    "zh": "Chinese",
}

_lang_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "lang"))
_translations = {}
_fallback = {}
_current_language = "en"


def _load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def _ensure_loaded():
    global _fallback
    if not _fallback:
        en_path = os.path.join(_lang_dir, "en.json")
        _fallback = _load_json(en_path)

    if not _translations:
        set_language(_current_language)


def set_language(code: str) -> bool:
    """Set the current language, falling back to English if unavailable."""
    global _translations, _current_language
    code = (code or "en").lower()
    if code not in SUPPORTED_LANGUAGES:
        code = "en"

    path = os.path.join(_lang_dir, f"{code}.json")
    data = _load_json(path)
    if not data and code != "en":
        code = "en"
        data = _load_json(os.path.join(_lang_dir, "en.json"))

    _translations = data or {}
    _current_language = code
    return True


def get_text(key: str) -> str:
    """Return translated text for key, or the key itself if missing."""
    _ensure_loaded()
    if not key:
        return ""
    if key in _translations:
        return _translations.get(key, key)
    if key in _fallback:
        return _fallback.get(key, key)
    return key


def get_available_languages() -> dict:
    """Return supported languages map: {code: name}."""
    return dict(SUPPORTED_LANGUAGES)


def get_current_language() -> str:
    return _current_language
