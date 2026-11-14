from core.browser_qt import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, 
                     QPushButton, Qt, QSize, QIcon, QFrame, pyqtSignal)
from core.browser_resources import ICONS

class MediaControlPopup(QFrame):
    """
    Popup widget for controlling media playback (Global Media Control style)
    """
    # Signals to send controls back to the browser
    play_pause_clicked = pyqtSignal()
    next_clicked = pyqtSignal()
    prev_clicked = pyqtSignal()
    
    def __init__(self, parent=None):
        super().__init__(parent, Qt.WindowType.Popup)
        self.setFixedWidth(350)
        self.setStyleSheet("""
            MediaControlPopup {
                background-color: #2D2D2D;
                border: 1px solid #3E3E3E;
                border-radius: 8px;
            }
            QLabel {
                color: #FFFFFF;
            }
            QPushButton {
                background-color: transparent;
                border: none;
                border-radius: 4px;
                padding: 5px;
            }
            QPushButton:hover {
                background-color: #3E3E3E;
            }
        """)
        self.setup_ui()
        
    def setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(15, 15, 15, 15)
        
        # Info row (Thumbnail + Text)
        info_layout = QHBoxLayout()
        
        # Thumbnail (placeholder)
        self.thumbnail_label = QLabel()
        self.thumbnail_label.setFixedSize(64, 64)
        self.thumbnail_label.setStyleSheet("background-color: #000; border-radius: 4px;")
        self.thumbnail_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.thumbnail_label.setText("🎵")
        info_layout.addWidget(self.thumbnail_label)
        
        # Text details
        text_layout = QVBoxLayout()
        self.title_label = QLabel("No media playing")
        self.title_label.setStyleSheet("font-weight: bold; font-size: 14px;")
        self.artist_label = QLabel("")
        self.artist_label.setStyleSheet("color: #AAAAAA; font-size: 12px;")
        
        text_layout.addWidget(self.title_label)
        text_layout.addWidget(self.artist_label)
        text_layout.addStretch()
        info_layout.addLayout(text_layout)
        
        layout.addLayout(info_layout)
        
        # Controls row
        controls_layout = QHBoxLayout()
        controls_layout.addStretch()
        
        self.btn_prev = QPushButton(ICONS.get('skip_prev', '⏮'))
        self.btn_prev.setFixedSize(32, 32)
        self.btn_prev.clicked.connect(self.prev_clicked.emit)
        
        self.btn_play = QPushButton(ICONS.get('play', '▶'))
        self.btn_play.setFixedSize(40, 40)
        self.btn_play.setStyleSheet("font-size: 20px;")
        self.btn_play.clicked.connect(self.play_pause_clicked.emit)
        
        self.btn_next = QPushButton(ICONS.get('skip_next', '⏭'))
        self.btn_next.setFixedSize(32, 32)
        self.btn_next.clicked.connect(self.next_clicked.emit)
        
        controls_layout.addWidget(self.btn_prev)
        controls_layout.addWidget(self.btn_play)
        controls_layout.addWidget(self.btn_next)
        controls_layout.addStretch()
        
        layout.addLayout(controls_layout)
        
    def update_metadata(self, metadata):
        """Update UI with metadata dict: {title, artist, artwork_url, paused}"""
        self.title_label.setText(metadata.get('title', 'Unknown Title'))
        self.artist_label.setText(metadata.get('artist', 'Unknown Artist'))
        
        # Update play/pause icon based on state
        is_paused = metadata.get('paused', True)
        self.btn_play.setText(ICONS.get('play', '▶') if is_paused else ICONS.get('pause', '⏸'))
        
        # TODO: Handle artwork URL loading if needed
        # For now, we keep the placeholder or set a pixmap if we download it
