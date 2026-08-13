import sys
import os
import ctypes
import traceback
from datetime import datetime

# Android/Mobile Detection
try:
    from android.permissions import request_permissions, Permission
    IS_ANDROID = True
except ImportError:
    IS_ANDROID = False
    
# If on Android, switch to Mobile Browser immediately
if IS_ANDROID:
    from core.mobile_browser import main as mobile_main
    if __name__ == "__main__":
        mobile_main()
        sys.exit(0)

from core.browser_qt import QApplication, Qt, QWebEngineUrlScheme, QIcon, QTimer
from core.browser_window import BrowserWindow
import core.browser_resources as browser_resources

DEBUG = os.environ.get("CLOUDAR_DEBUG", "0").lower() in {"1", "true", "yes", "on"}

def debug_log(message):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{timestamp}] {message}"
    print(line, flush=True)
    try:
        log_path = os.path.join(browser_resources.ensure_data_directory(), "debug.log")
        with open(log_path, "a", encoding="utf-8") as log_file:
            log_file.write(line + "\n")
    except Exception as exc:
        try:
            print(f"[debug] Failed to write debug log: {exc}", flush=True)
        except Exception:
            pass

def log_exception(exc_type, exc_value, exc_traceback):
    details = "".join(traceback.format_exception(exc_type, exc_value, exc_traceback))
    debug_log("UNCAUGHT EXCEPTION\n" + details)
    sys.__excepthook__(exc_type, exc_value, exc_traceback)

sys.excepthook = log_exception

def register_schemes():
    """Register custom URL schemes before QApplication starts"""
    # Register cloudar:// scheme for internal pages
    scheme = QWebEngineUrlScheme(b"cloudar")
    scheme.setFlags(QWebEngineUrlScheme.Flag.LocalAccessAllowed | 
                   QWebEngineUrlScheme.Flag.SecureScheme | 
                   QWebEngineUrlScheme.Flag.ContentSecurityPolicyIgnored)
    QWebEngineUrlScheme.registerScheme(scheme)
    
    # Register cloudar-asset:// scheme for user assets (backgrounds, etc.)
    asset_scheme = QWebEngineUrlScheme(b"cloudar-asset")
    asset_scheme.setFlags(QWebEngineUrlScheme.Flag.LocalAccessAllowed | 
                          QWebEngineUrlScheme.Flag.SecureScheme | 
                          QWebEngineUrlScheme.Flag.ContentSecurityPolicyIgnored)
    QWebEngineUrlScheme.registerScheme(asset_scheme)

def main():
    try:
        debug_log("Starting Cloudar Browser")
        debug_log(f"Python version: {sys.version.split()[0]}")
        debug_log(f"Working directory: {os.getcwd()}")
        debug_log(f"Script path: {__file__}")
        debug_log(f"Debug logging enabled: {DEBUG}")

        # Register custom schemes first
        register_schemes()
        debug_log("Registered cloudar:// URL scheme")
        
        # Ensure data directory exists
        data_dir = browser_resources.ensure_data_directory(browser_resources.BROWSER_DATA_DIR)
        debug_log(f"Data directory: {os.path.abspath(data_dir)}")
        
        # Ensure forced download directory exists if enabled
        try:
            settings = browser_resources.load_json_file(browser_resources.SETTINGS_FILE, browser_resources.DEFAULT_SETTINGS)
            if settings.get("force_download_directory", False):
                forced_path = settings.get("forced_download_path", "")
                if forced_path:
                    os.makedirs(forced_path, exist_ok=True)
                    debug_log(f"Forced download directory ensured: {forced_path}")
        except Exception as exc:
            debug_log(f"Failed to ensure forced download directory: {exc}")
        
        # Enable high DPI scaling
        QApplication.setHighDpiScaleFactorRoundingPolicy(
            Qt.HighDpiScaleFactorRoundingPolicy.PassThrough
        )
        
        app = QApplication(sys.argv)
        debug_log("QApplication initialized")
        app.setApplicationName("Cloudar Browser™")
        app.setOrganizationName("Cloudar")
        
        # Set AppUserModelID for Windows taskbar icon
        if os.name == 'nt':
            myappid = 'cloudar.browser.main.1.0'  # Arbitrary string
            try:
                ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(myappid)
                debug_log("Windows AppUserModelID set")
            except Exception as exc:
                debug_log(f"Failed to set Windows AppUserModelID: {exc}")
                
        # Set application icon
        icon_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'resources', 'Icon.ico')
        if os.path.exists(icon_path):
            app.setWindowIcon(QIcon(icon_path))
            debug_log(f"Application icon loaded: {icon_path}")
        else:
            debug_log(f"Application icon not found: {icon_path}")
        
        # Create and show main window
        window = BrowserWindow()
        debug_log("BrowserWindow created")
        window.show()
        debug_log("BrowserWindow shown")
        
        debug_log("Starting Qt event loop")
        exit_code = app.exec()
        debug_log(f"Qt event loop exited with code: {exit_code}")
        return exit_code
    except Exception:
        debug_log("Startup failed\n" + traceback.format_exc())
        raise


if __name__ == "__main__":
    sys.exit(main())

