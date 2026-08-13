# Quick Start Guide - Hyggshi Browser

## Installation

```bash
# 1. Install dependencies
pip install PyQt6 PyQt6-WebEngine

# 2. Run the browser
python main.py
```

## Windows DLL Error Fix

If you get `ImportError: DLL load failed`:

1. **Download Visual C++ Redistributable:**
   - https://aka.ms/vs/17/release/vc_redist.x64.exe

2. **Install and restart** your computer

3. **Run again:**
   ```bash
   python main.py
   ```

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+T` | New tab |
| `Ctrl+W` | Close tab |
| `Ctrl+D` | Bookmark page |
| `Ctrl+H` | Show history |
| `Ctrl+J` | Show downloads |
| `Ctrl+Q` | Quit |
| `F5` | Reload page |

## Features

✅ Multiple tabs with favicons  
✅ Smart address bar (URL + search)  
✅ Bookmarks with star button  
✅ Browsing history  
✅ Download manager  
✅ Dark theme UI  
✅ Customizable settings  

## Usage Tips

- **Search:** Just type in the address bar (uses Google)
- **Navigate:** Use back/forward buttons or mouse buttons
- **Bookmark:** Click the star icon in address bar
- **New Tab:** Click the + button or press Ctrl+T
- **Settings:** Menu → Tools → Settings

## File Locations

All data is stored in `browser_data/`:
- `bookmarks.json` - Your bookmarks
- `history.json` - Browsing history
- `settings.json` - Preferences
- `downloads.json` - Download history

## Customization

**Change home page:**
1. Menu → Tools → Settings
2. Enter new URL in "Home Page" field
3. Click Save

**Change download location:**
1. Menu → Tools → Settings
2. Click "Browse..." next to Download Location
3. Select folder and click Save

## Support

For issues, check the full README.md or the walkthrough.md documentation.
