from core.browser_qt import (QWidget, QVBoxLayout, QHBoxLayout, QComboBox,
                        QToolButton, QWebEngineView, QUrl, QDockWidget,
                        Qt, QIcon, QSize, QTabWidget, QPushButton, QTextEdit,
                        QApplication, QLabel, QLineEdit, QScrollArea)
from core.browser_resources import ICONS
from features.language_manager import LanguageManager
from core.language import get_text

class AISidebar(QDockWidget):
    """
    A sidebar widget that allows accessing various AI agents (Chat)
    and Mini Task Agents behaviors (read-only automation).
    """
    PROVIDERS = {
        "ChatGPT": "https://chatgpt.com/",
        "Gemini": "https://gemini.google.com/",
        "Claude": "https://claude.ai/",
        "DeepSeek": "https://chat.deepseek.com/",
        "Copilot": "https://copilot.microsoft.com/",
        "Perplexity": "https://www.perplexity.ai/",
        "Blackbox": "https://www.blackbox.ai/",
        "HuggingChat": "https://huggingface.co/chat/",
        "Poe": "https://poe.com/"
    }

    GOOGLE_SERVICES = [
        ("G", "Google", "https://www.google.com/"),
        ("M", "Gmail", "https://mail.google.com/"),
        ("D", "Drive", "https://drive.google.com/"),
        ("▶", "YouTube", "https://www.youtube.com/"),
        ("⌖", "Maps", "https://www.google.com/maps"),
        ("◫", "Photos", "https://photos.google.com/"),
        ("≡", "Docs", "https://docs.google.com/"),
        ("▣", "Calendar", "https://calendar.google.com/"),
        ("文", "Translate", "https://translate.google.com/"),
        ("✓", "Keep", "https://keep.google.com/"),
    ]

    def __init__(self, browser_window):
        self.lang = LanguageManager.instance()
        tr = get_text
        super().__init__(tr("ai_sidebar_title"), browser_window)
        self.browser_window = browser_window
        self.setFeatures(QDockWidget.DockWidgetFeature.DockWidgetClosable |
                         QDockWidget.DockWidgetFeature.DockWidgetMovable)
        self.setAllowedAreas(Qt.DockWidgetArea.RightDockWidgetArea |
                             Qt.DockWidgetArea.LeftDockWidgetArea)

        # Main Container
        container = QWidget()
        main_layout = QVBoxLayout(container)
        main_layout.setContentsMargins(0, 0, 0, 0)

        # Tabs
        self.tabs = QTabWidget()
        self.tabs.setDocumentMode(True)
        main_layout.addWidget(self.tabs)

        # --- Tab 1: Chat ---
        self.chat_widget = QWidget()
        self.setup_chat_tab()
        self._chat_tab_index = self.tabs.addTab(self.chat_widget, tr("ai_chat_tab"))

        # --- Tab 2: Agent ---
        self.agent_widget = QWidget()
        self.setup_agent_tab()
        self._agent_tab_index = self.tabs.addTab(self.agent_widget, tr("ai_agent_tab"))

        # --- Tab 3: Google ---
        self.google_widget = QWidget()
        self.setup_google_tab()
        self._google_tab_index = self.tabs.addTab(self.google_widget, "Google")

        self.setWidget(container)
        self.setMinimumWidth(350)
        
    def setup_chat_tab(self):
        layout = QVBoxLayout(self.chat_widget)
        layout.setContentsMargins(0, 0, 0, 0)
        
        # Header (Provider Switcher & Controls)
        header = QWidget()
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(5, 5, 5, 5)
        
        self.provider_combo = QComboBox()
        self.provider_combo.addItems(self.PROVIDERS.keys())
        self.provider_combo.currentTextChanged.connect(self.load_provider)
        header_layout.addWidget(self.provider_combo, 1)
        
        self.back_btn = QToolButton()
        self.back_btn.setText("←")
        self.back_btn.setToolTip("Back")
        self.back_btn.clicked.connect(lambda: self.webview.back())
        
        self.reload_btn = QToolButton()
        self.reload_btn.setText("↻")
        self.reload_btn.setToolTip("Reload")
        self.reload_btn.clicked.connect(lambda: self.webview.reload())
        
        header_layout.addWidget(self.back_btn)
        header_layout.addWidget(self.reload_btn)
        
        layout.addWidget(header)
        
        # Web View
        self.webview = QWebEngineView()
        self.webview.setUrl(QUrl(self.PROVIDERS["ChatGPT"]))
        layout.addWidget(self.webview)

    def setup_agent_tab(self):
        tr = get_text
        layout = QVBoxLayout(self.agent_widget)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(10)

        self.agent_info_label = QLabel(f"<b>{tr('ai_task_agent')}</b><br>{tr('ai_task_agent_desc')}")
        self.agent_info_label.setWordWrap(True)
        layout.addWidget(self.agent_info_label)

        # Actions
        self.btn_summarize = QPushButton(tr("ai_summarize"))
        self.btn_summarize.clicked.connect(self.action_summarize)
        layout.addWidget(self.btn_summarize)

        self.btn_translate = QPushButton(tr("ai_translate"))
        self.btn_translate.clicked.connect(self.action_translate)
        layout.addWidget(self.btn_translate)

        self.btn_explain = QPushButton(tr("ai_explain_code"))
        self.btn_explain.setToolTip(tr("ai_explain_code_tooltip"))
        self.btn_explain.clicked.connect(self.action_explain_code)
        layout.addWidget(self.btn_explain)

        self.btn_links = QPushButton(tr("ai_extract_links"))
        self.btn_links.clicked.connect(self.action_extract_links)
        layout.addWidget(self.btn_links)

        layout.addStretch()

        # Output Area
        self.agent_output_label = QLabel(tr("ai_output_label"))
        layout.addWidget(self.agent_output_label)
        self.agent_output = QTextEdit()
        self.agent_output.setReadOnly(True)
        layout.addWidget(self.agent_output)

    def setup_google_tab(self):
        container = self.google_widget
        layout = QVBoxLayout(container)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(10)

        search_row = QHBoxLayout()
        self.google_search_input = QLineEdit()
        self.google_search_input.setObjectName("GoogleSearchInput")
        self.google_search_input.setPlaceholderText("Search Google")
        self.google_search_input.returnPressed.connect(self.open_google_search)
        search_row.addWidget(self.google_search_input, 1)

        self.google_search_button = QPushButton("Search")
        self.google_search_button.setObjectName("GoogleSearchButton")
        self.google_search_button.clicked.connect(self.open_google_search)
        search_row.addWidget(self.google_search_button)
        layout.addLayout(search_row)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.Shape.NoFrame)
        scroll.setObjectName("GoogleSidebarScroll")

        services_widget = QWidget()
        services_layout = QVBoxLayout(services_widget)
        services_layout.setContentsMargins(0, 0, 0, 0)
        services_layout.setSpacing(8)

        for icon, label, url in self.GOOGLE_SERVICES:
            btn = QPushButton(f"{icon}  {label}")
            btn.setObjectName("GoogleServiceButton")
            btn.setToolTip(url)
            btn.clicked.connect(lambda _, service_url=url, service_title=label: self.open_google_service(service_url, service_title))
            services_layout.addWidget(btn)

        services_layout.addStretch()
        scroll.setWidget(services_widget)
        layout.addWidget(scroll, 1)

        container.setStyleSheet("""
            QLineEdit#GoogleSearchInput {
                border: 1px solid #5f6368;
                border-radius: 12px;
                padding: 8px 12px;
                background: #303134;
                color: #e8eaed;
            }
            QPushButton#GoogleSearchButton,
            QPushButton#GoogleServiceButton {
                border: 1px solid #5f6368;
                border-radius: 12px;
                padding: 8px 12px;
                background: transparent;
                color: #e8eaed;
            }
            QPushButton#GoogleSearchButton:hover,
            QPushButton#GoogleServiceButton:hover {
                background: #3c4043;
                border-color: #8ab4f8;
            }
            QPushButton#GoogleServiceButton {
                text-align: left;
                font-weight: 600;
            }
        """)

        self.google_widget_layout_root = container
        self.google_widget_layout = layout
        self.google_services_widget = services_widget
        self.google_services_layout = services_layout

    def open_google_search(self):
        query = self.google_search_input.text().strip()
        if not query:
            self.google_search_input.setFocus()
            return
        url = f"https://www.google.com/search?q={QUrl.toPercentEncoding(query).data().decode('utf-8')}"
        self.browser_window.add_new_tab(QUrl(url), title=f"Google: {query}")

    def open_google_service(self, url, title):
        self.browser_window.add_new_tab(QUrl(url), title=title)

    def load_provider(self, name):
        url = self.PROVIDERS.get(name)
        if url:
            self.webview.setUrl(QUrl(url))
            
    def toggle_sidebar(self):

        """
        Update all translatable strings when the language changes.
        Called by BrowserWindow.retranslate_ui() during runtime language switch.
        """
        tr = get_text

        # Dock widget title
        self.setWindowTitle(tr("ai_sidebar_title"))

        # Tab titles
        self.tabs.setTabText(self._chat_tab_index, tr("ai_chat_tab"))
        self.tabs.setTabText(self._agent_tab_index, tr("ai_agent_tab"))
        self.tabs.setTabText(self._google_tab_index, "Google")

        # Agent tab widgets
        self.agent_info_label.setText(f"<b>{tr('ai_task_agent')}</b><br>{tr('ai_task_agent_desc')}")
        self.btn_summarize.setText(tr("ai_summarize"))
        self.btn_translate.setText(tr("ai_translate"))
        self.btn_explain.setText(tr("ai_explain_code"))
        self.btn_explain.setToolTip(tr("ai_explain_code_tooltip"))
        self.btn_links.setText(tr("ai_extract_links"))
        self.agent_output_label.setText(tr("ai_output_label"))

    def get_current_page(self):
        browser = self.browser_window.current_browser()
        return browser.page() if browser else None

    def action_summarize(self):
        page = self.get_current_page()
        if not page: return

        self.agent_output.setText(get_text("ai_reading_page"))
        page.runJavaScript(
            "document.body.innerText",
            lambda text: self.process_text_prompt(text, "Please summarize the following webpage content concisely:\n\n")
        )

    def action_translate(self):
        page = self.get_current_page()
        if not page: return

        self.agent_output.setText(get_text("ai_reading_page"))
        page.runJavaScript(
            "document.body.innerText",
            lambda text: self.process_text_prompt(text, "Please translate the following webpage content to English (or user's language):\n\n")
        )

    def action_explain_code(self):
        page = self.get_current_page()
        if not page: return

        self.agent_output.setText(get_text("ai_reading_selection"))
        page.runJavaScript(
            "window.getSelection().toString()",
            lambda text: self.process_text_prompt(text, "Please explain this code snippet:\n\n")
        )

    def process_text_prompt(self, text, prefix):
        tr = get_text
        if not text or len(text.strip()) == 0:
            self.agent_output.setText(tr("ai_no_text"))
            return

        limit = 10000
        if len(text) > limit:
            text = text[:limit] + "\n...[Truncated]"

        full_prompt = prefix + text

        clipboard = QApplication.clipboard()
        clipboard.setText(full_prompt)

        self.agent_output.setText(tr("ai_prompt_copied"))

    def action_extract_links(self):
        page = self.get_current_page()
        if not page: return

        self.agent_output.setText(get_text("ai_extracting_links"))
        js = """
        (function() {
            var links = Array.from(document.querySelectorAll('a'));
            return links.map(a => a.href).filter(h => h.startswith('http')).join('\\n');
        })();
        """
        page.runJavaScript(js, self.display_extracted_links)

    def display_extracted_links(self, result):
        tr = get_text
        if not result:
            self.agent_output.setText(tr("ai_no_links"))
        else:
            self.agent_output.setText(f"{tr('ai_links_found')}\n\n{result}")
