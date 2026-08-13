# Hyggshi Browser

A modern, feature-rich web browser built with **PyQt6** and **QWebEngine**, inspired by Google Chrome and Comet Browser.

![Python](https://img.shields.io/badge/Python-3.8+-blue.svg)
![PyQt6](https://img.shields.io/badge/PyQt6-6.6+-green.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)

## ✨ Features

- **🗂️ Tabbed Browsing** - Multiple tabs with easy switching and management
- **🔍 Smart Address Bar** - URL autocomplete and integrated search
- **⭐ Bookmark Management** - Save and organize your favorite sites
- **📜 Browsing History** - Track and revisit your browsing history
- **⬇️ Download Manager** - Track downloads with progress indicators
- **🎨 Modern Dark Theme** - Sleek, Chrome-inspired dark interface
- **⚙️ Customizable Settings** - Configure home page, search engine, and more
- **🔒 HTTPS Support** - Secure browsing with SSL/TLS
- **⌨️ Keyboard Shortcuts** - Efficient navigation with hotkeys

## 📋 Requirements

- Python 3.8 or higher
- PyQt6
- PyQt6-WebEngine

## 🚀 Installation

1. **Clone or download this repository**

2. **Install dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

   Or install manually:
   ```bash
   pip install PyQt6 PyQt6-WebEngine
   ```

3. **Run the browser:**
   ```bash
   python main.py
   ```

> [!IMPORTANT]
> **Windows Users:** If you encounter a `DLL load failed` error, you need to install the **Visual C++ Redistributable**:
> 1. Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe
> 2. Install and restart your computer
> 3. Run `python main.py` again
> 
> See the [Troubleshooting](#-troubleshooting) section for more details.

## 🎮 Usage

### Keyboard Shortcuts

- `Ctrl+T` - New tab
- `Ctrl+W` - Close current tab
- `Ctrl+N` - New window
- `Ctrl+D` - Add bookmark
- `Ctrl+H` - Show history
- `Ctrl+J` - Show downloads
- `Ctrl+Q` - Quit browser
- `F5` - Reload page
- `Ctrl+L` - Focus address bar

### Navigation

- **Address Bar**: Enter URLs or search queries
- **Back/Forward**: Navigate through page history
- **Home**: Return to your home page
- **Reload**: Refresh the current page
- **Bookmark Star**: Add/remove current page from bookmarks

### Features

#### Bookmarks
- Click the star icon in the address bar to bookmark the current page
- Access bookmarks via the Bookmarks menu
- Double-click a bookmark to navigate to it

#### History
- All visited pages are automatically saved to history
- View history via `Ctrl+H` or the History menu
- Search through your browsing history
- Clear history when needed

#### Downloads
- Downloads are automatically tracked
- View download progress and history via `Ctrl+J`
- Downloads are saved to your configured download location

#### Settings
- Configure your home page
- Set your preferred search engine
- Choose download location
- Access via Tools → Settings

## 📁 Project Structure

```
HyggshiWebEngine/
├── main.py                 # Application entry point
├── browser_window.py       # Main browser window
├── web_view.py            # Custom web view widget
├── bookmark_manager.py    # Bookmark management
├── history_manager.py     # History tracking
├── download_manager.py    # Download handling
├── settings_dialog.py     # Settings/preferences
├── styles.py              # UI stylesheet
├── resources.py           # Resource management
├── requirements.txt       # Python dependencies
├── browser_data/          # User data directory
│   ├── bookmarks.json    # Saved bookmarks
│   ├── history.json      # Browsing history
│   ├── settings.json     # User preferences
│   └── downloads.json    # Download records
└── README.md             # This file
```

## 🎨 Customization

### Changing the Theme

Edit `styles.py` to customize colors and styling. The browser uses Qt Style Sheets (QSS) for theming.

### Default Settings

Modify `DEFAULT_SETTINGS` in `resources.py` to change default values:

```python
DEFAULT_SETTINGS = {
    "home_page": "https://www.google.com",
    "search_engine": "https://www.google.com/search?q={}",
    "download_location": os.path.expanduser("~/Downloads"),
    "theme": "dark",
    "show_bookmark_bar": True
}
```

## 🔧 Troubleshooting

### Browser won't start
- Ensure PyQt6 and PyQt6-WebEngine are installed correctly
- Check Python version (3.8+ required)

### Pages won't load
- Check your internet connection
- Verify firewall settings aren't blocking the browser

### Downloads not working
- Check download location permissions
- Ensure the download directory exists

## 🤝 Contributing

Contributions are welcome! Feel free to:
- Report bugs
- Suggest new features
- Submit pull requests

## 📄 License

This project is open source and available under the MIT License.

## 🙏 Acknowledgments

- Built with [PyQt6](https://www.riverbankcomputing.com/software/pyqt/)
- Powered by [QtWebEngine](https://doc.qt.io/qt-6/qtwebengine-index.html)
- Inspired by Google Chrome and Comet Browser

## 📧 Contact

For questions or support, please open an issue on the project repository.

---

**Enjoy browsing with Hyggshi Browser! 🚀**
