"""
Find bar widget for searching text in web pages
"""
from core.browser_qt import (QWidget, QHBoxLayout, QLineEdit, QPushButton,
                     QLabel, QToolButton, pyqtSignal, Qt, QIcon, QWebEnginePage)
from features.language_manager import LanguageManager
from core.language import get_text

class FindBar(QWidget):
    """Widget for finding text in the current page"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.web_view = None
        self.lang = LanguageManager.instance()
        self.setup_ui()
        self.hide()

    def setup_ui(self):
        tr = get_text
        layout = QHBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(5)

        # Close button
        self.close_btn = QToolButton()
        self.close_btn.setText("×")
        self.close_btn.clicked.connect(self.hide_bar)
        self.close_btn.setStyleSheet("QToolButton { border: none; font-weight: bold; }")
        layout.addWidget(self.close_btn)

        # Search input
        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText(tr("find_placeholder"))
        self.search_input.returnPressed.connect(self.find_next)
        self.search_input.textChanged.connect(self.find_next)
        layout.addWidget(self.search_input)

        # Navigation buttons
        self.prev_btn = QToolButton()
        self.prev_btn.setText("▲")
        self.prev_btn.setToolTip(tr("find_previous"))
        self.prev_btn.clicked.connect(self.find_previous)
        layout.addWidget(self.prev_btn)

        self.next_btn = QToolButton()
        self.next_btn.setText("▼")
        self.next_btn.setToolTip(tr("find_next"))
        self.next_btn.clicked.connect(self.find_next)
        layout.addWidget(self.next_btn)
        
        # Label to match styling
        self.setStyleSheet("""
            FindBar {
                background-color: #2b2b2b;
                border: 1px solid #444746;
                border-radius: 8px;
                margin: 5px;
            }
            QLineEdit {
                border-radius: 4px;
                background-color: #1e1e1e;
                padding: 4px 8px;
            }
            QToolButton {
                border-radius: 4px;
                padding: 4px;
            }
            QToolButton:hover {
                background-color: #3c4043;
            }
        """)
        
        self.setLayout(layout)
        
    def set_web_view(self, web_view):
        """Set the current web view target"""
        self.web_view = web_view
        
    def show_bar(self):
        """Show the find bar and focus input"""
        self.show()
        self.search_input.setFocus()
        self.search_input.selectAll()
        
    def hide_bar(self):
        """Hide the find bar and clear search"""
        self.hide()
        if self.web_view:
            self.web_view.findText("")
            
    def find_next(self):
        """Find next occurrence"""
        text = self.search_input.text()
        if self.web_view and text:
            self.web_view.findText(text)
            
    def find_previous(self):
        """Find previous occurrence"""
        text = self.search_input.text()
        if self.web_view and text:
            self.web_view.findText(text, QWebEnginePage.FindFlag.FindBackward)

    def retranslate_ui(self):
        """Update translatable strings when language changes"""
        tr = get_text
        self.search_input.setPlaceholderText(tr("find_placeholder"))
        self.prev_btn.setToolTip(tr("find_previous"))
        self.next_btn.setToolTip(tr("find_next"))
