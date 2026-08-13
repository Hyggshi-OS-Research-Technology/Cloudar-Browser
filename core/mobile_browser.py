"""
Mobile/Android entry point for Cloudar Browser™.
Note: PyQt6-WebEngine (Chromium) is not available on Android.
This version acts as a lightweight launcher/dashboard that opens links
in the system's default browser or uses available native views.
"""
import sys
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QLabel, QPushButton, QListWidget, QMessageBox)
from PyQt6.QtCore import QUrl, Qt, QSize
from PyQt6.QtGui import QDesktopServices, QIcon, QFont

class MobileBrowser(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Cloudar Mobile")
        self.setup_ui()
        
    def setup_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        layout.setSpacing(20)
        
        # Header
        title = QLabel("Cloudar")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("font-size: 32px; font-weight: bold; color: #60cdff; margin-top: 40px;")
        layout.addWidget(title)
        
        subtitle = QLabel("Mobile Edition")
        subtitle.setAlignment(Qt.AlignmentFlag.AlignCenter)
        subtitle.setStyleSheet("font-size: 18px; color: #a0a0a0;")
        layout.addWidget(subtitle)
        
        layout.addSpacing(20)
        
        # Search/URL Simulation (Opens in System Browser)
        btn_search = QPushButton("ðŸ” Search / Enter URL")
        btn_search.setMinimumHeight(60)
        btn_search.setStyleSheet("""
            QPushButton {
                background-color: #2b2b2b;
                color: white;
                border-radius: 30px;
                font-size: 18px;
                text-align: left;
                padding-left: 20px;
            }
        """)
        btn_search.clicked.connect(self.open_google)
        layout.addWidget(btn_search)
        
        # Shortcuts Grid (Simulated with List for simplicity)
        shortcuts_label = QLabel("Quick Access")
        shortcuts_label.setStyleSheet("font-size: 20px; font-weight: bold; color: white;")
        layout.addWidget(shortcuts_label)
        
        self.shortcuts = [
            ("Google", "https://google.com"),
            ("YouTube", "https://youtube.com"),
            ("GitHub", "https://github.com"),
            ("Reddit", "https://reddit.com"),
            ("Cloudar Home", "https://example.com/cloudar")
        ]
        
        for name, url in self.shortcuts:
            btn = QPushButton(name)
            btn.setMinimumHeight(80)
            btn.setStyleSheet("""
                QPushButton {
                    background-color: #3e3e3e;
                    color: white;
                    border-radius: 15px;
                    font-size: 18px;
                    margin-bottom: 10px;
                }
                QPushButton:pressed {
                    background-color: #60cdff;
                    color: black;
                }
            """)
            btn.clicked.connect(lambda checked, u=url: self.open_system_browser(u))
            layout.addWidget(btn)
            
        layout.addStretch()
        
        # Footer
        footer = QLabel("Powered by Python & BeeWare")
        footer.setAlignment(Qt.AlignmentFlag.AlignCenter)
        footer.setStyleSheet("color: #606060; font-size: 12px; margin-bottom: 20px;")
        layout.addWidget(footer)

    def open_google(self):
        self.open_system_browser("https://google.com")
        
    def open_system_browser(self, url_str):
        QDesktopServices.openUrl(QUrl(url_str))

def main():
    app = QApplication(sys.argv)
    window = MobileBrowser()
    window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()

