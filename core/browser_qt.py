"""
Standardized Compatibility Layer for PyQt6 and PyQt5
"""
import sys

# Try PyQt6 first
try:
    from PyQt6.QtCore import Qt, QUrl, QSize, QTimer, QObject, pyqtSignal, pyqtSlot, QEvent, QPoint, QByteArray, QBuffer, QIODevice
    from PyQt6.QtWidgets import (QApplication, QMainWindow, QTabWidget, QToolBar, 
                                 QLineEdit, QPushButton, QStatusBar, QMenu, QMenuBar, 
                                 QMessageBox, QWidget, QVBoxLayout, QHBoxLayout, 
                                 QLabel, QFileDialog, QGroupBox, QFormLayout, 
                                 QListWidget, QToolButton, QDialog, QTableWidget, 
                                 QTableWidgetItem, QHeaderView, QTabBar, QCheckBox, QComboBox,
                                 QStackedWidget, QListWidgetItem, QFrame, QSlider, QSplitter, QDockWidget, QTextEdit, QScrollArea,
                                 QSizePolicy)
    from PyQt6.QtGui import QAction, QIcon, QKeySequence, QPixmap, QDesktopServices
    
    from PyQt6.QtWebEngineCore import (QWebEngineProfile, QWebEnginePage, QWebEngineSettings,
                                       QWebEngineUrlScheme, QWebEngineUrlSchemeHandler,
                                       QWebEngineUrlRequestJob, QWebEngineUrlRequestInterceptor)
    from PyQt6.QtWebEngineWidgets import QWebEngineView
    from PyQt6.QtWebEngineCore import QWebEngineScript
    from PyQt6.QtWebChannel import QWebChannel
    
    print("Using PyQt6")
    QtVersion = 6

except ImportError:
    try:
        # Try PyQt5
        from PyQt5.QtCore import Qt, QUrl, QSize, QTimer, QObject, pyqtSignal, pyqtSlot, QEvent, QPoint, QByteArray, QBuffer, QIODevice
        from PyQt5.QtWidgets import (QApplication, QMainWindow, QTabWidget, QToolBar, 
                                     QLineEdit, QPushButton, QStatusBar, QMenu, QMenuBar, 
                                     QMessageBox, QWidget, QVBoxLayout, QHBoxLayout, 
                                     QLabel, QFileDialog, QGroupBox, QFormLayout, 
                                     QListWidget, QToolButton, QAction, QDialog,
                                     QTableWidget, QTableWidgetItem, QHeaderView, QTabBar,
                                     QCheckBox, QComboBox, QStackedWidget, QListWidgetItem, QFrame, QSlider, QSplitter, QDockWidget, QTextEdit, QScrollArea,
                                     QSizePolicy)
        from PyQt5.QtGui import QIcon, QKeySequence, QPixmap, QDesktopServices
        
        from PyQt5.QtWebEngineWidgets import QWebEngineView, QWebEngineProfile, QWebEnginePage, QWebEngineSettings
        from PyQt5.QtWebEngineCore import (QWebEngineUrlScheme, QWebEngineUrlSchemeHandler,
                                            QWebEngineScript, QWebEngineUrlRequestJob,
                                            QWebEngineUrlRequestInterceptor)
        from PyQt5.QtWebChannel import QWebChannel
        
        print("Using PyQt5")
        QtVersion = 5
        
        # Patch QKeySequence enums
        if not hasattr(QKeySequence, 'StandardKey'):
             class StandardKey:
                 ZoomIn = QKeySequence.ZoomIn
                 ZoomOut = QKeySequence.ZoomOut
                 Find = QKeySequence.Find
             QKeySequence.StandardKey = StandardKey
             
    except ImportError as e:
        print("Critical Error: neither PyQt6 nor PyQt5 could be imported!")
        print(f"Details: {e}")
        sys.exit(1)




