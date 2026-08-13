"""
Experimental Flags manager for Cloudar Browser™ (cloudar://flags)

Mirrors the spirit of chrome://flags: a registry of experimental / in-progress
features that can be toggled on or off without digging into Settings. Flags
default to a safe value and persist across restarts; some require a restart
to fully take effect (flagged via "restart_required").
"""
from core.browser_resources import FLAGS_FILE, load_json_file, save_json_file

# ── Flag registry ───────────────────────────────────────────────────────
# id:        stable key used everywhere else in the codebase to check state
# title/description: shown on the cloudar://flags page (translation keys)
# default:   value used the first time the flag is seen
# category:  grouping shown on the page
# restart_required: show a "Relaunch" banner when toggled
FLAG_DEFINITIONS = [
    {
        "id": "download-bubble",
        "title_key": "flag_download_bubble_title",
        "desc_key": "flag_download_bubble_desc",
        "category": "UI",
        "default": True,
        "restart_required": False,
    },
    {
        "id": "vertical-tabs-default",
        "title_key": "flag_vertical_tabs_title",
        "desc_key": "flag_vertical_tabs_desc",
        "category": "UI",
        "default": False,
        "restart_required": True,
    },
    {
        "id": "tab-hover-preview",
        "title_key": "flag_tab_hover_title",
        "desc_key": "flag_tab_hover_desc",
        "category": "UI",
        "default": False,
        "restart_required": False,
    },
    {
        "id": "smooth-scrolling",
        "title_key": "flag_smooth_scroll_title",
        "desc_key": "flag_smooth_scroll_desc",
        "category": "Performance",
        "default": True,
        "restart_required": True,
    },
    {
        "id": "gpu-rasterization",
        "title_key": "flag_gpu_raster_title",
        "desc_key": "flag_gpu_raster_desc",
        "category": "Performance",
        "default": False,
        "restart_required": True,
    },
    {
        "id": "parallel-downloads",
        "title_key": "flag_parallel_downloads_title",
        "desc_key": "flag_parallel_downloads_desc",
        "category": "Downloads",
        "default": False,
        "restart_required": False,
    },
    {
        "id": "adblock-v2-engine",
        "title_key": "flag_adblock_v2_title",
        "desc_key": "flag_adblock_v2_desc",
        "category": "Privacy",
        "default": False,
        "restart_required": True,
    },
    {
        "id": "ai-sidebar-streaming",
        "title_key": "flag_ai_streaming_title",
        "desc_key": "flag_ai_streaming_desc",
        "category": "AI",
        "default": True,
        "restart_required": False,
    },
]

_FLAGS_BY_ID = {f["id"]: f for f in FLAG_DEFINITIONS}


class FlagsManager:
    """Load, query, and persist experimental flag overrides."""

    def __init__(self, persist=True, data_file=None):
        self.persist = bool(persist)
        self.data_file = data_file or FLAGS_FILE
        self._overrides = load_json_file(self.data_file, default={}) if self.persist else {}

    def _save(self):
        if self.persist:
            save_json_file(self.data_file, self._overrides)

    def is_enabled(self, flag_id: str) -> bool:
        """Return the current effective state of a flag (override or default)."""
        if flag_id in self._overrides:
            return bool(self._overrides[flag_id])
        definition = _FLAGS_BY_ID.get(flag_id)
        return bool(definition["default"]) if definition else False

    def set_flag(self, flag_id: str, enabled: bool) -> bool:
        """Persist a user override for a flag. Returns False for unknown ids."""
        if flag_id not in _FLAGS_BY_ID:
            return False
        self._overrides[flag_id] = bool(enabled)
        self._save()
        return True

    def reset_flag(self, flag_id: str) -> bool:
        """Remove any override, reverting the flag to its default value."""
        if flag_id in self._overrides:
            del self._overrides[flag_id]
            self._save()
            return True
        return False

    def reset_all(self):
        self._overrides = {}
        self._save()

    def any_restart_required_changed(self) -> bool:
        """True if any restart-required flag currently differs from its default."""
        for definition in FLAG_DEFINITIONS:
            if not definition["restart_required"]:
                continue
            if definition["id"] in self._overrides and \
                    bool(self._overrides[definition["id"]]) != bool(definition["default"]):
                return True
        return False

    def get_all(self):
        """Return the full flag list with current effective values, for the UI."""
        result = []
        for definition in FLAG_DEFINITIONS:
            item = dict(definition)
            item["enabled"] = self.is_enabled(definition["id"])
            item["is_override"] = definition["id"] in self._overrides
            result.append(item)
        return result
