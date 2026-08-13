import os
import json
from core.browser_qt import QObject, QWebEngineScript, pyqtSignal

class ExtensionManager(QObject):
    """Manages browser extensions (user scripts and styles)"""

    extensions_changed = pyqtSignal()
    
    def __init__(self, data_dir):
        super().__init__()
        self.data_dir = data_dir
        self.extensions_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "extensions")
        self.config_file = os.path.join(data_dir, "extensions.json")
        self.scripts = [] # List of QWebEngineScript
        self.extensions = [] # List of extension metadata dicts
        
        if not os.path.exists(self.extensions_dir):
            os.makedirs(self.extensions_dir)
            
        self.load_extensions()

    def load_extensions(self):
        """Load all extensions from extensions/<id>/manifest.json."""
        self.scripts = []
        self.extensions = []
        
        # Load user configuration (enabled/disabled)
        config = {}
        if os.path.exists(self.config_file):
            try:
                with open(self.config_file, 'r') as f:
                    config = json.load(f)
            except:
                pass

        seen_ids = set()

        for entry in os.listdir(self.extensions_dir):
            ext_dir = os.path.join(self.extensions_dir, entry)
            if not os.path.isdir(ext_dir):
                continue

            manifest_path = os.path.join(ext_dir, "manifest.json")
            if not os.path.exists(manifest_path):
                continue

            try:
                with open(manifest_path, "r", encoding="utf-8") as f:
                    manifest = json.load(f)
            except Exception as e:
                print(f"Error reading manifest for {entry}: {e}")
                continue

            ext_id = entry
            seen_ids.add(ext_id)
            name = manifest.get("name", ext_id)
            description = manifest.get("description", "")
            version = manifest.get("version", "1.0.0")
            enabled = bool(config.get(ext_id, True))

            # Optional: sites this extension runs on ("matches", Chrome
            # match-pattern syntax, see core/url_match.py) and the
            # QWebChannel bridge object name it needs exposed there
            # ("bridge"). Both are declared in the manifest so adding a
            # new extension that needs a page-side bridge never requires
            # touching core/browser_window.py's routing logic.
            matches = manifest.get("matches", [])
            if isinstance(matches, str):
                matches = [matches]
            bridge = manifest.get("bridge")

            extension = {
                "id": ext_id,
                "name": name,
                "description": description,
                "version": version,
                "enabled": enabled,
                "path": ext_dir,
                "matches": matches,
                "bridge": bridge,
            }
            self.extensions.append(extension)

            if not enabled:
                continue

            script_path = manifest.get("background", "background.js")
            script_file = os.path.join(ext_dir, script_path)
            if not os.path.exists(script_file):
                print(f"Extension {ext_id} missing {script_path}")
                continue

            try:
                with open(script_file, "r", encoding="utf-8") as f:
                    code = f.read()

                script = QWebEngineScript()
                script.setSourceCode(code)
                script.setName(f"ext:{ext_id}")
                script.setWorldId(QWebEngineScript.ScriptWorldId.MainWorld)
                script.setInjectionPoint(QWebEngineScript.InjectionPoint.DocumentReady)
                script.setRunsOnSubFrames(True)

                self.scripts.append(script)
                print(f"Loaded extension: {name}")
            except Exception as e:
                print(f"Error loading extension {ext_id}: {e}")

        # Legacy support: single .js files in extensions/ (no manifest)
        for filename in os.listdir(self.extensions_dir):
            if not filename.endswith(".js"):
                continue
            ext_id = os.path.splitext(filename)[0]
            if ext_id in seen_ids:
                continue

            enabled = bool(config.get(ext_id, True))
            extension = {
                "id": ext_id,
                "name": ext_id,
                "description": "Legacy script",
                "version": "1.0.0",
                "enabled": enabled,
                "path": os.path.join(self.extensions_dir, filename),
                "matches": [],
                "bridge": None,
            }
            self.extensions.append(extension)
            if not enabled:
                continue
            try:
                with open(extension["path"], "r", encoding="utf-8") as f:
                    code = f.read()
                script = QWebEngineScript()
                script.setSourceCode(code)
                script.setName(f"ext:{ext_id}")
                script.setWorldId(QWebEngineScript.ScriptWorldId.MainWorld)
                script.setInjectionPoint(QWebEngineScript.InjectionPoint.DocumentReady)
                script.setRunsOnSubFrames(True)
                self.scripts.append(script)
                print(f"Loaded legacy extension: {ext_id}")
            except Exception as e:
                print(f"Error loading legacy extension {ext_id}: {e}")

        self.extensions_changed.emit()

    def get_scripts(self):
        """Return list of scripts to be added to a QWebEnginePage/Profile"""
        return self.scripts

    def get_script_names(self):
        return [f"ext:{ext['id']}" for ext in self.extensions]

    def get_extensions(self):
        return self.extensions

    def toggle_extension(self, ext_id, enabled):
        """Enable or disable an extension and save state"""
        config = {}
        if os.path.exists(self.config_file):
            try:
                with open(self.config_file, 'r') as f:
                    config = json.load(f)
            except:
                pass
        
        config[ext_id] = enabled
        
        try:
            with open(self.config_file, 'w') as f:
                json.dump(config, f, indent=4)
            self.load_extensions() # Refresh active scripts
        except Exception as e:
            print(f"Error saving extension config: {e}")
