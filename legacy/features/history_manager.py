"""
Browsing history management
"""
from datetime import datetime
from core.browser_resources import HISTORY_FILE, load_json_file, save_json_file, ensure_data_directory


class HistoryManager:
    """Manage browsing history"""
    
    def __init__(self):
        self.history = load_json_file(HISTORY_FILE, default=[])
        self.max_history_items = 1000
    
    def add_history(self, title, url):
        """Add a page to history"""
        # Don't add empty URLs or duplicates from the same session
        if not url or url.startswith("about:") or url.startswith("cloudar://"):
            return
        
        history_item = {
            "title": title,
            "url": url,
            "visited": datetime.now().isoformat()
        }
        
        # Add to beginning of list
        self.history.insert(0, history_item)
        
        # Limit history size
        if len(self.history) > self.max_history_items:
            self.history = self.history[:self.max_history_items]
        
        self.save()
    
    def get_history(self, limit=100):
        """Get recent history items"""
        return self.history[:limit]
    
    def search_history(self, query):
        """Search history by title or URL"""
        query = query.lower()
        return [
            h for h in self.history
            if query in (h.get("title") or "").lower() or query in (h.get("url") or "").lower()
        ]
    
    def clear_history(self):
        """Clear all history"""
        self.history = []
        self.save()
    
    def save(self):
        """Save history to file"""
        save_json_file(HISTORY_FILE, self.history)


class MemoryHistoryManager(HistoryManager):
    """In-memory history store (no disk persistence)."""

    def __init__(self):
        self.history = []
        self.max_history_items = 1000

    def save(self):
        """No-op for memory-only history."""
        pass

from core.browser_qt import QDialog, QVBoxLayout, QTableWidget, QTableWidgetItem, QHeaderView, QLabel, QPushButton, QHBoxLayout, QUrl, Qt

class HistoryDialog(QDialog):
    """Dialog to show browsing history"""
    
    def __init__(self, history_manager, browser_window, parent=None):
        super().__init__(parent)
        self.history_manager = history_manager
        self.browser_window = browser_window
        self.setWindowTitle("History")
        self.setMinimumSize(700, 500)
        self.setup_ui()
        
    def setup_ui(self):
        """Setup the UI"""
        layout = QVBoxLayout()
        
        # Title
        title = QLabel("History")
        title.setStyleSheet("font-size: 20px; font-weight: bold; margin-bottom: 10px; color: #ffffff;")
        layout.addWidget(title)
        
        # History Table
        self.table = QTableWidget()
        self.table.setColumnCount(3)
        self.table.setHorizontalHeaderLabels(["Title", "URL", "Date"])
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        self.table.verticalHeader().setVisible(False)
        self.table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.table.setShowGrid(False)
        self.table.setAlternatingRowColors(True)
        self.table.setStyleSheet("""
            QTableWidget {
                background-color: #2b2b2b;
                border: 1px solid #3e3e3e;
                border-radius: 6px;
                gridline-color: #3e3e3e;
            }
            QHeaderView::section {
                background-color: #202020;
                padding: 6px;
                border: none;
                border-bottom: 1px solid #3e3e3e;
                font-weight: bold;
            }
        """)
        
        self.table.itemDoubleClicked.connect(self.navigate_to_item)
        
        self.load_history()
        layout.addWidget(self.table)
        
        # Buttons
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        refresh_btn = QPushButton("Refresh")
        refresh_btn.clicked.connect(self.load_history)
        button_layout.addWidget(refresh_btn)
        
        clear_btn = QPushButton("Clear History")
        clear_btn.clicked.connect(self.clear_history)
        button_layout.addWidget(clear_btn)
        
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.close)
        button_layout.addWidget(close_btn)
        
        layout.addLayout(button_layout)
        self.setLayout(layout)
        
    def load_history(self):
        """Load history into the table"""
        self.table.setRowCount(0)
        history = self.history_manager.get_history(100)
        
        self.table.setRowCount(len(history))
        
        for i, item in enumerate(history):
            # Title
            title_item = QTableWidgetItem(item["title"])
            self.table.setItem(i, 0, title_item)
            
            # URL
            url_item = QTableWidgetItem(item["url"])
            self.table.setItem(i, 1, url_item)
            
            # Date
            date_str = item.get("visited", "").replace("T", " ").split(".")[0]
            date_item = QTableWidgetItem(date_str)
            self.table.setItem(i, 2, date_item)
            
    def navigate_to_item(self, item):
        """Navigate to selected history item"""
        row = item.row()
        url = self.table.item(row, 1).text()
        if self.browser_window:
            self.browser_window.add_new_tab(QUrl(url))
            self.close()
            
    def clear_history(self):
        """Clear history"""
        from core.browser_qt import QMessageBox
        reply = QMessageBox.question(
            self, "Clear History",
            "Are you sure you want to clear all browsing history?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            self.history_manager.clear_history()
            self.load_history()

