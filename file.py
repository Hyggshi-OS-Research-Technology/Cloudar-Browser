"""
organize_project.py
--------------------
Tu dong to chuc lai folder cho project "Hyggshi Browser Python" (Cloudar Browser).

CACH DUNG:
1. Dat file nay vao thu muc goc cua project (cung cap voi adblock.py, browser_qt.py, ...)
2. Chay thu (an toan, khong doi gi):
       python organize_project.py
   -> Mac dinh DRY_RUN = True, chi in ra nhung gi SE lam.
3. Khi xem log thay OK, sua DRY_RUN = False roi chay lai de thuc thi thuc su.
4. Script tu dong backup toan bo project ra file zip truoc khi move file.

Mapping co the sua trong FILE_MAP ben duoi cho phu hop voi project cua ban.
"""

import os
import re
import shutil
import zipfile
from datetime import datetime

# ====== CONFIG ======
DRY_RUN = False  # doi thanh False khi muon chay thuc su

ROOT = os.path.dirname(os.path.abspath(__file__))

# file -> folder dich (relative to ROOT)
FILE_MAP = {}
FILE_MAP.update({
    # core - logic chinh cua browser
    "web_view.py": "core",
    "tab_widget.py": "core",
    "split_view.py": "core",
    "mobile_browser.py": "core",
    "styles.py": "core",

    # features - cac chuc nang phu
    "download_manager.py": "features",
    "extension_manager.py": "features",
    "find_bar.py": "features",
    "history_manager.py": "features",
    "media_control.py": "features",
    "performance_manager.py": "features",
    "permissions_manager.py": "features",
    "session_manager.py": "features",
    "settings_backend.py": "features",
    "settings_dialog.py": "features",
    "language_manager.py": "features",
    "internal_handler.py": "features",
    "internal_pages_bridge.py": "features",

    # build/tools
    "fix_install.bat": "build",
    "run.bat": "build",
    "CloudarBrowser.spec": "build",
    "HyggshiBrowser.spec": "build",
    "convert_icon.py": "tools",

    # docs
    "README.md": "docs",
    "QUICKSTART.md": "docs",

    # tests
    "test_frag.py": "tests",
})

# Cac folder se duoc tao thanh python package (co __init__.py)
PYTHON_PACKAGES = {"core", "features"}

# Cac file la python module se duoc cap nhat import sau khi move
PY_MODULES = {
    name[:-3]: target
    for name, target in FILE_MAP.items()
    if name.endswith(".py")
}

# Thu muc/file bo qua khi quet sua import
IGNORE_DIRS = {"__pycache__", ".git", ".venv", "venv", "dist", "build_backup"}


def log(msg: str):
    print(msg)


def make_backup():
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_name = f"project_backup_{timestamp}.zip"
    backup_path = os.path.join(ROOT, backup_name)

    if DRY_RUN:
        log(f"[DRY-RUN] Se tao backup: {backup_name}")
        return

    log(f"Dang tao backup: {backup_name} ...")
    with zipfile.ZipFile(backup_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for dirpath, dirnames, filenames in os.walk(ROOT):
            # khong backup chinh file backup hoac __pycache__/dist
            dirnames[:] = [d for d in dirnames if d not in IGNORE_DIRS]
            for f in filenames:
                if f.startswith("project_backup_") and f.endswith(".zip"):
                    continue
                full = os.path.join(dirpath, f)
                rel = os.path.relpath(full, ROOT)
                zf.write(full, rel)
    log(f"Backup hoan tat: {backup_path}")


def create_folders():
    for folder in set(FILE_MAP.values()):
        folder_path = os.path.join(ROOT, folder)
        if not os.path.exists(folder_path):
            if DRY_RUN:
                log(f"[DRY-RUN] Se tao folder: {folder}/")
            else:
                os.makedirs(folder_path, exist_ok=True)
                log(f"Da tao folder: {folder}/")

        if folder in PYTHON_PACKAGES:
            init_path = os.path.join(folder_path, "__init__.py")
            if not os.path.exists(init_path):
                if DRY_RUN:
                    log(f"[DRY-RUN] Se tao file: {folder}/__init__.py")
                else:
                    with open(init_path, "w", encoding="utf-8") as f:
                        f.write("")
                    log(f"Da tao file: {folder}/__init__.py")


def move_files():
    for filename, folder in FILE_MAP.items():
        src = os.path.join(ROOT, filename)
        dst = os.path.join(ROOT, folder, filename)

        if not os.path.exists(src):
            log(f"[SKIP] Khong tim thay file: {filename}")
            continue

        if os.path.exists(dst):
            log(f"[SKIP] Da ton tai o dich: {folder}/{filename}")
            continue

        if DRY_RUN:
            log(f"[DRY-RUN] Se move: {filename} -> {folder}/{filename}")
        else:
            shutil.move(src, dst)
            log(f"Da move: {filename} -> {folder}/{filename}")


def build_import_patterns():
    """
    Tra ve list (regex_pattern, replace_func) cho moi module da move.
    Xu ly cac dang:
        import features.adblock
        import features.adblock as ab
        from features.adblock import Something
        from features.adblock import Something as Alias
    -> chuyen thanh import qua package moi, ví du 'features.adblock'
    """
    patterns = []

    for module_name, folder in PY_MODULES.items():
        new_path = f"{folder}.{module_name}"

        # from <module> import X [as Y]
        pat_from = re.compile(
            rf"(^\s*from\s+){re.escape(module_name)}(\s+import\s+)",
            re.MULTILINE,
        )
        patterns.append((pat_from, rf"\g<1>{new_path}\g<2>"))

        # import <module> [as alias]
        pat_import = re.compile(
            rf"(^\s*import\s+){re.escape(module_name)}(\s+as\s+\w+|\s*$)",
            re.MULTILINE,
        )
        patterns.append((pat_import, rf"\g<1>{new_path}\g<2>"))

    return patterns


def fix_imports():
    patterns = build_import_patterns()
    if not patterns:
        return

    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in IGNORE_DIRS]

        for filename in filenames:
            if not filename.endswith(".py"):
                continue

            filepath = os.path.join(dirpath, filename)

            try:
                with open(filepath, "r", encoding="utf-8") as f:
                    content = f.read()
            except UnicodeDecodeError:
                log(f"[SKIP] Khong doc duoc (encoding): {filepath}")
                continue

            new_content = content
            for pattern, repl in patterns:
                new_content = pattern.sub(repl, new_content)

            if new_content != content:
                rel = os.path.relpath(filepath, ROOT)
                if DRY_RUN:
                    log(f"[DRY-RUN] Se sua import trong: {rel}")
                else:
                    with open(filepath, "w", encoding="utf-8") as f:
                        f.write(new_content)
                    log(f"Da sua import trong: {rel}")


def main():
    log("==== TO CHUC LAI PROJECT: Hyggshi Browser Python ====")
    log(f"DRY_RUN = {DRY_RUN}")
    log("")

    log("1) Backup project")
    make_backup()
    log("")

    log("2) Tao folder")
    create_folders()
    log("")

    log("3) Move file")
    move_files()
    log("")

    log("4) Sua import trong cac file .py")
    fix_imports()
    log("")

    if DRY_RUN:
        log("==> Day la DRY-RUN. Chua co gi thay doi thuc su.")
        log("==> Sua DRY_RUN = False trong file nay roi chay lai de thuc thi.")
    else:
        log("==> Hoan tat! Kiem tra lai project va chay thu de chac chan moi thu hoat dong.")


if __name__ == "__main__":
    main()