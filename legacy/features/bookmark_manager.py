"""
Bookmark management system
"""
from datetime import datetime
from core.browser_resources import BOOKMARKS_FILE, load_json_file, save_json_file


class BookmarkManager:
    """Manage browser bookmarks"""
    
    def __init__(self):
        self.bookmarks = []
        self.folders = []
        self._load()

    def _load(self):
        data = load_json_file(BOOKMARKS_FILE, default={"bookmarks": [], "folders": []})

        if isinstance(data, list):
            self.bookmarks = data
            folder_names = []
            for item in self.bookmarks:
                folder = item.get("folder", "")
                if folder and folder not in folder_names:
                    folder_names.append(folder)
            self.folders = [{"name": name, "created": datetime.now().isoformat()} for name in folder_names]
            return

        if isinstance(data, dict):
            self.bookmarks = data.get("bookmarks", data.get("items", [])) or []
            raw_folders = data.get("folders", [])
            folders = []
            if raw_folders and isinstance(raw_folders, list):
                if isinstance(raw_folders[0], dict):
                    folders = [f for f in raw_folders if f.get("name")]
                else:
                    folders = [{"name": f, "created": datetime.now().isoformat()} for f in raw_folders if isinstance(f, str)]

            folder_names = {f["name"] for f in folders}
            for item in self.bookmarks:
                folder = item.get("folder", "")
                if folder and folder not in folder_names:
                    folders.append({"name": folder, "created": datetime.now().isoformat()})
                    folder_names.add(folder)

            self.folders = folders
            return

        self.bookmarks = []
        self.folders = []
    
    def add_bookmark(self, title, url, folder=""):
        """Add a new bookmark"""
        if folder:
            self.add_folder(folder)
        bookmark = {
            "title": title,
            "url": url,
            "folder": folder,
            "created": datetime.now().isoformat()
        }
        self.bookmarks.append(bookmark)
        self.save()
        return True
    
    def remove_bookmark(self, url):
        """Remove a bookmark by URL"""
        self.bookmarks = [b for b in self.bookmarks if b["url"] != url]
        self.save()
    
    def is_bookmarked(self, url):
        """Check if URL is bookmarked"""
        return any(b["url"] == url for b in self.bookmarks)
    
    def get_bookmarks(self, folder=None):
        """Get all bookmarks or bookmarks in a specific folder"""
        if folder is None:
            return self.bookmarks
        return [b for b in self.bookmarks if b.get("folder", "") == folder]

    def get_folders(self):
        """Return list of folder names."""
        return [f["name"] for f in self.folders]

    def add_folder(self, name):
        """Create a bookmark folder if it doesn't exist."""
        name = (name or "").strip()
        if not name:
            return False
        if any(f["name"] == name for f in self.folders):
            return False
        self.folders.append({"name": name, "created": datetime.now().isoformat()})
        self.save()
        return True

    def remove_folder(self, name, remove_bookmarks=True):
        """Remove a folder (optionally delete bookmarks inside it)."""
        name = (name or "").strip()
        if not name:
            return False
        self.folders = [f for f in self.folders if f["name"] != name]
        if remove_bookmarks:
            self.bookmarks = [b for b in self.bookmarks if b.get("folder", "") != name]
        else:
            for item in self.bookmarks:
                if item.get("folder", "") == name:
                    item["folder"] = ""
        self.save()
        return True
    
    def save(self):
        """Save bookmarks to file"""
        data = {
            "version": 2,
            "folders": self.folders,
            "bookmarks": self.bookmarks
        }
        save_json_file(BOOKMARKS_FILE, data)
