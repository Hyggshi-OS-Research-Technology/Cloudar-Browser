"""
Permissions manager for Cloudar Browser™.
Stores per-origin decisions for camera, microphone, location, notifications.
"""
from core.browser_resources import PERMISSIONS_FILE, load_json_file, save_json_file


class PermissionsManager:
    """Manage site permission decisions."""

    def __init__(self, persist=True, data_file=None):
        self.persist = bool(persist)
        self.data_file = data_file or PERMISSIONS_FILE
        self.permissions = load_json_file(self.data_file, default={}) if self.persist else {}

    def _save(self):
        if self.persist:
            save_json_file(self.data_file, self.permissions)

    def get_permission(self, origin, permission):
        origin = (origin or "").strip()
        permission = (permission or "").strip()
        if not origin or not permission:
            return "ask"
        return self.permissions.get(origin, {}).get(permission, "ask")

    def set_permission(self, origin, permission, value):
        origin = (origin or "").strip()
        permission = (permission or "").strip()
        value = (value or "ask").strip()
        if not origin or not permission:
            return False
        if origin not in self.permissions:
            self.permissions[origin] = {}
        self.permissions[origin][permission] = value
        self._save()
        return True

    def get_all(self):
        return self.permissions

    def clear(self):
        self.permissions = {}
        self._save()

    def remove_origin(self, origin):
        origin = (origin or "").strip()
        if origin in self.permissions:
            self.permissions.pop(origin, None)
            self._save()
            return True
        return False

