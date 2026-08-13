"""
Browser stylesheets
A modern, minimal theme (Arc/Edge-inspired): floating pill tabs, a quiet
toolbar that blends into the window until you interact with it, a single
accent color, generous rounding, and light borders instead of heavy chrome.
"""

COLORS = {
    "dark": {
        "window": "#17171a",
        "toolbar": "#17171a",
        "content": "#1c1c1f",
        "surface": "#202024",
        "menu": "#1f1f23",
        "input": "#232327",
        "omnibox": "#232327",
        "omnibox_border": "#2e2e34",
        "tab": "transparent",
        "tab_hover": "#232327",
        "tab_selected": "#232327",
        "text": "#f2f2f3",
        "text_muted": "#9a9aa2",
        "accent": "#9083f7",
        "accent_muted": "#3a3560",
        "selection": "#34305a",
        "selection_text": "#ffffff",
        "hover": "#232327",
        "pressed": "#2b2b30",
        "border": "#2e2e34",
        "line": "#232327",
    },
    "light": {
        "window": "#f6f6f8",
        "toolbar": "#f6f6f8",
        "content": "#ffffff",
        "surface": "#ffffff",
        "menu": "#ffffff",
        "input": "#ffffff",
        "omnibox": "#ffffff",
        "omnibox_border": "#e3e3e8",
        "tab": "transparent",
        "tab_hover": "#ececf1",
        "tab_selected": "#ffffff",
        "text": "#1c1c1e",
        "text_muted": "#6b6b76",
        "accent": "#6c5ce7",
        "accent_muted": "#ece9fd",
        "selection": "#ece9fd",
        "selection_text": "#1c1c1e",
        "hover": "#ececf1",
        "pressed": "#e1e1e8",
        "border": "#e3e3e8",
        "line": "#ececf1",
    },
    "incognito": {
        "window": "#18151f",
        "toolbar": "#18151f",
        "content": "#1d1a26",
        "surface": "#221e2d",
        "menu": "#201c29",
        "input": "#26212f",
        "omnibox": "#26212f",
        "omnibox_border": "#332d3f",
        "tab": "transparent",
        "tab_hover": "#26212f",
        "tab_selected": "#26212f",
        "text": "#efeaf7",
        "text_muted": "#a49bb5",
        "accent": "#a48cf7",
        "accent_muted": "#3d3452",
        "selection": "#3c3252",
        "selection_text": "#ffffff",
        "hover": "#26212f",
        "pressed": "#2e2839",
        "border": "#332d3f",
        "line": "#26212f",
    },
}


def get_stylesheet(theme="dark"):
    c = COLORS.get(theme, COLORS["dark"])

    return f"""
    QMainWindow {{
        background-color: {c['window']};
        color: {c['text']};
    }}

    QWidget {{
        background-color: {c['window']};
        color: {c['text']};
        selection-background-color: {c['selection']};
        selection-color: {c['selection_text']};
        font-family: 'Segoe UI Variable', 'Segoe UI', Arial, sans-serif;
        font-size: 10.5pt;
    }}

    QLabel {{
        background: transparent;
        color: {c['text']};
    }}

    QMainWindow::separator {{
        background: transparent;
        width: 0px;
        height: 0px;
    }}

    QToolBar {{
        border: none;
    }}

    QMenuBar#ChromeMenuBar {{
        background-color: {c['toolbar']};
        border: none;
        padding: 8px 10px 2px 10px;
        spacing: 2px;
    }}

    QMenuBar#ChromeMenuBar::item {{
        background: transparent;
        color: {c['text_muted']};
        border-radius: 8px;
        padding: 6px 10px;
        margin: 0 1px;
    }}

    QMenuBar#ChromeMenuBar::item:selected {{
        background-color: {c['hover']};
        color: {c['text']};
    }}

    QToolBar#ChromeNavigationBar {{
        background-color: {c['toolbar']};
        border: none;
        padding: 10px 14px 10px 14px;
        spacing: 10px;
    }}

    QToolBar#ChromeNavigationBar::separator {{
        width: 0px;
    }}

    QWidget#ChromeNavControls,
    QWidget#ChromeToolbarActions {{
        background: transparent;
    }}

    QToolButton#ChromeNavButton,
    QToolButton#ChromeToolbarButton,
    QToolButton#ChromeMenuButton,
    QToolButton#ChromeNewTabButton,
    QToolButton#ChromeOmniboxAction {{
        background: transparent;
        border: none;
        border-radius: 10px;
        color: {c['text_muted']};
        min-width: 30px;
        min-height: 30px;
        padding: 0;
        font-size: 14px;
        font-weight: 500;
    }}

    QToolButton#ChromeNavButton:hover,
    QToolButton#ChromeToolbarButton:hover,
    QToolButton#ChromeMenuButton:hover,
    QToolButton#ChromeNewTabButton:hover,
    QToolButton#ChromeOmniboxAction:hover {{
        background-color: {c['hover']};
        color: {c['text']};
    }}

    QToolButton#ChromeNavButton:pressed,
    QToolButton#ChromeToolbarButton:pressed,
    QToolButton#ChromeMenuButton:pressed,
    QToolButton#ChromeNewTabButton:pressed,
    QToolButton#ChromeOmniboxAction:pressed {{
        background-color: {c['pressed']};
    }}

    QToolButton#ChromeNavButton:disabled {{
        color: {c['border']};
        background: transparent;
    }}

    QToolButton#ChromeMenuButton {{
        font-size: 17px;
    }}

    QToolButton#ChromeNewTabButton {{
        font-size: 16px;
        font-weight: 600;
        margin: 6px 10px 6px 0;
        background: transparent;
        color: {c['accent']};
        border-radius: 10px;
    }}

    QToolButton#ChromeNewTabButton:hover {{
        background-color: {c['accent_muted']};
        color: {c['accent']};
    }}

    QToolButton#ChromeNewTabButton:pressed {{
        background-color: {c['accent_muted']};
        color: {c['accent']};
    }}

    QFrame#ChromeOmnibox {{
        background-color: {c['omnibox']};
        border: 1.5px solid {c['omnibox_border']};
        border-radius: 20px;
    }}

    QFrame#ChromeOmnibox:hover {{
        border-color: {c['text_muted']};
    }}

    QLabel#ChromeOmniboxStatus {{
        color: {c['text_muted']};
        font-size: 11pt;
        padding: 0 2px;
    }}

    QLineEdit {{
        background-color: {c['input']};
        border: 1.5px solid {c['border']};
        border-radius: 14px;
        padding: 6px 12px;
        color: {c['text']};
        selection-background-color: {c['selection']};
        selection-color: {c['selection_text']};
    }}

    QLineEdit:hover {{
        border-color: {c['accent']};
    }}

    QLineEdit:focus {{
        background-color: {c['content']};
        border: 1.5px solid {c['accent']};
    }}

    QLineEdit#ChromeUrlBar {{
        background: transparent;
        border: none;
        padding: 8px 0;
        color: {c['text']};
        font-size: 10.5pt;
    }}

    QLineEdit#ChromeUrlBar:hover,
    QLineEdit#ChromeUrlBar:focus {{
        background: transparent;
        border: none;
    }}

    QTabWidget#ChromeTabWidget::pane {{
        border: none;
        background-color: {c['content']};
    }}

    QTabBar#ChromeTabBar {{
        background-color: {c['toolbar']};
        border: none;
        padding: 4px 8px 0 8px;
    }}

    QTabBar#ChromeTabBar::tab {{
        background-color: {c['tab']};
        color: {c['text_muted']};
        padding: 9px 16px;
        border: 1px solid transparent;
        border-radius: 12px;
        min-width: 160px;
        max-width: 232px;
        margin: 4px 3px 6px 3px;
    }}

    QTabBar#ChromeTabBar::tab:selected {{
        background-color: {c['tab_selected']};
        color: {c['text']};
        border-color: {c['border']};
    }}

    QTabBar#ChromeTabBar::tab:hover:!selected {{
        background-color: {c['tab_hover']};
        color: {c['text']};
    }}

    QTabBar#ChromeTabBar::close-button {{
        image: url(close_icon.svg);
        subcontrol-position: right;
        padding: 2px;
        margin-left: 8px;
        border-radius: 8px;
    }}

    QTabBar#ChromeTabBar::close-button:hover {{
        background: {c['hover']};
    }}

    QToolBar#BookmarksBar {{
        background-color: {c['content']};
        border-top: 1px solid {c['line']};
        border-bottom: 1px solid {c['line']};
        padding: 5px 12px;
        spacing: 4px;
    }}

    QFrame#SplitHeader,
    QFrame#SplitPaneBar {{
        background-color: {c['toolbar']};
        border-bottom: 1px solid {c['line']};
    }}

    QLabel#SplitHeaderTitle {{
        color: {c['text_muted']};
        font-size: 10pt;
        font-weight: 600;
    }}

    QToolButton#SplitHeaderButton,
    QToolButton#SplitPaneButton {{
        background: transparent;
        border: none;
        border-radius: 10px;
        color: {c['text']};
        padding: 0 10px;
    }}

    QToolButton#SplitHeaderButton:hover,
    QToolButton#SplitPaneButton:hover {{
        background-color: {c['hover']};
    }}

    QToolButton#SplitHeaderButton:pressed,
    QToolButton#SplitPaneButton:pressed {{
        background-color: {c['pressed']};
    }}

    QLineEdit#SplitPaneUrlBar {{
        background-color: {c['omnibox']};
        border: 1.5px solid {c['omnibox_border']};
        border-radius: 16px;
        padding: 6px 14px;
    }}

    QSplitter#SplitBrowserSplitter::handle {{
        background-color: {c['line']};
    }}

    QToolBar#BookmarksBar QToolButton {{
        background: transparent;
        border: none;
        border-radius: 8px;
        padding: 6px 10px;
        color: {c['text']};
    }}

    QToolBar#BookmarksBar QToolButton:hover {{
        background-color: {c['hover']};
    }}

    QToolBar#BookmarksBar QToolButton:pressed {{
        background-color: {c['pressed']};
    }}

    QLabel#BookmarkBarHint {{
        color: {c['text_muted']};
        padding: 0 6px;
    }}

    QPushButton {{
        background-color: transparent;
        border: 1.5px solid {c['border']};
        border-radius: 14px;
        padding: 7px 18px;
        color: {c['text']};
        font-weight: 500;
    }}

    QPushButton:hover {{
        background-color: {c['hover']};
        border-color: {c['accent']};
    }}

    QPushButton:pressed {{
        background-color: {c['pressed']};
    }}

    QPushButton:default {{
        background-color: {c['accent']};
        border-color: {c['accent']};
        color: #ffffff;
    }}

    QPushButton:default:hover {{
        background-color: {c['accent']};
        color: #ffffff;
    }}

    QMenu {{
        background-color: {c['menu']};
        border: 1px solid {c['border']};
        border-radius: 14px;
        padding: 8px;
    }}

    QMenu::item {{
        padding: 8px 28px 8px 14px;
        border-radius: 9px;
        margin: 2px 4px;
    }}

    QMenu::item:selected {{
        background-color: {c['accent_muted']};
        color: {c['text']};
    }}

    QMenu::separator {{
        height: 1px;
        background: {c['line']};
        margin: 6px 10px;
    }}

    QDockWidget {{
        background-color: {c['surface']};
        border-left: 1px solid {c['line']};
        titlebar-close-icon: url(close_icon.svg);
    }}

    QDockWidget::title {{
        background-color: {c['surface']};
        padding: 12px;
        font-weight: 600;
    }}

    QFrame#IncognitoBanner {{
        background-color: {c['surface']};
        border-bottom: 1px solid {c['line']};
        padding: 6px 12px;
    }}

    QLabel#IncognitoBannerTitle {{
        font-size: 12px;
        font-weight: 600;
        color: {c['text']};
    }}

    QLabel#IncognitoBannerText {{
        font-size: 11px;
        color: {c['text_muted']};
    }}

    QScrollBar:vertical {{
        background: transparent;
        width: 8px;
        margin: 2px;
    }}

    QScrollBar::handle:vertical {{
        background: {c['border']};
        min-height: 24px;
        border-radius: 4px;
        margin: 0px;
    }}

    QScrollBar::handle:vertical:hover {{
        background: {c['text_muted']};
    }}

    QScrollBar:horizontal {{
        background: transparent;
        height: 8px;
        margin: 2px;
    }}

    QScrollBar::handle:horizontal {{
        background: {c['border']};
        min-width: 24px;
        border-radius: 4px;
        margin: 0px;
    }}

    QScrollBar::handle:horizontal:hover {{
        background: {c['text_muted']};
    }}

    QScrollBar::add-line,
    QScrollBar::sub-line {{
        height: 0;
        width: 0;
    }}

    QScrollBar::add-page,
    QScrollBar::sub-page {{
        background: transparent;
    }}

    QStatusBar {{
        background-color: {c['content']};
        color: {c['text_muted']};
        border-top: 1px solid {c['line']};
        font-size: 10px;
    }}

    QLabel#AdBlockStatus {{
        color: {c['accent']};
        padding: 0 8px;
        font-weight: 600;
    }}

    QLabel#RAMStatus {{
        color: {c['text_muted']};
        padding: 0 8px;
        font-weight: 500;
    }}
    """


BROWSER_STYLESHEET = get_stylesheet("dark")
