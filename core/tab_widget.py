"""
Custom TabWidget with enhanced functionality (context menus, middle-click close)
"""
from core.browser_qt import (
    QAction,
    QMenu,
    QSize,
    QTabBar,
    QTabWidget,
    Qt,
    QWidget,
    pyqtSignal,
)

class TabBar(QTabBar):
    """Custom TabBar to handle middle-click closing"""
    
    # Signal for middle click closing
    tab_middle_clicked = pyqtSignal(int)
    tab_hover_entered = pyqtSignal()
    tab_hover_left = pyqtSignal()
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMovable(True)
        self.setTabsClosable(True)
        self.setObjectName("ChromeTabBar")
        self.setDrawBase(False)
        self.setExpanding(False)
        self.setUsesScrollButtons(False)
        self.setIconSize(QSize(16, 16))
        self._compact_mode = False
        self._tab_texts = {}

    def set_compact_mode(self, compact):
        if compact == self._compact_mode:
            return

        self._compact_mode = compact
        if compact:
            self._tab_texts = {
                index: self.tabText(index)
                for index in range(self.count())
            }
            for index in range(self.count()):
                super().setTabText(index, "")
            self.setIconSize(QSize(20, 20))
        else:
            for index, text in self._tab_texts.items():
                if index < self.count():
                    super().setTabText(index, text)
            self._tab_texts.clear()
            self.setIconSize(QSize(16, 16))
        self.update()

    def setTabText(self, index, text):
        if self._compact_mode:
            self._tab_texts[index] = text
            super().setTabText(index, "")
        else:
            super().setTabText(index, text)

    def insertTab(self, *args):
        index = super().insertTab(*args)
        if self._compact_mode:
            self._tab_texts[index] = self.tabText(index)
            super().setTabText(index, "")
        return index

    def mousePressEvent(self, event):

        """Handle mouse press events"""
        try:
            middle_button = Qt.MouseButton.MiddleButton
        except AttributeError:
            middle_button = Qt.MiddleButton

        if event.button() == middle_button:
            # Check if clicked on a tab
            if hasattr(event, "position"):
                tab_pos = event.position().toPoint()
            else:
                tab_pos = event.pos()
            tab_index = self.tabAt(tab_pos)
            if tab_index != -1:
                self.tab_middle_clicked.emit(tab_index)
                return
                
        super().mousePressEvent(event)

    def enterEvent(self, event):
        super().enterEvent(event)
        self.tab_hover_entered.emit()

    def leaveEvent(self, event):
        super().leaveEvent(event)
        self.tab_hover_left.emit()


class TabWidget(QTabWidget):
    """Custom TabWidget with context menus and improved interaction"""
    
    # Signal for new tab request (e.g. from context menu)
    new_tab_requested = pyqtSignal()
    # Signal to duplicate a tab
    duplicate_tab_requested = pyqtSignal(int)
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("ChromeTabWidget")
        
        # Use custom tab bar
        self.tab_bar = TabBar(self)
        self.tab_bar.tab_middle_clicked.connect(self.close_tab)
        self.setTabBar(self.tab_bar)
        
        self.setTabsClosable(True)
        self.setMovable(True)
        self.setDocumentMode(True)
        
        # Context menu policy
        try:
            context_policy = Qt.ContextMenuPolicy.CustomContextMenu
        except AttributeError:
            context_policy = Qt.CustomContextMenu
        self.setContextMenuPolicy(context_policy)
        self.customContextMenuRequested.connect(self.show_context_menu)
        
    def close_tab(self, index):
        """Close tab at index (proxy to parent functionality via signal usually, but we handle basic closing)"""
        if index != -1:
            self.tabCloseRequested.emit(index)

    def show_context_menu(self, position):
        """Show context menu for tabs"""
        # Find which tab was clicked
        # map position to tab bar coordinates
        tab_bar_pos = self.tabBar().mapFrom(self, position)
        index = self.tabBar().tabAt(tab_bar_pos)
        
        menu = QMenu(self)
        
        # Actions always available
        new_tab_action = QAction("New Tab", self)
        new_tab_action.triggered.connect(self.new_tab_requested.emit)
        menu.addAction(new_tab_action)
        
        menu.addSeparator()
        
        if index != -1:
            # Tab specific actions
            
            # Duplicate
            duplicate_action = QAction("Duplicate Tab", self)
            duplicate_action.triggered.connect(lambda: self.duplicate_tab_requested.emit(index))
            menu.addAction(duplicate_action)
            
            # Reload
            reload_action = QAction("Reload Tab", self)
            reload_action.triggered.connect(lambda: self.reload_tab(index))
            menu.addAction(reload_action)
            
            menu.addSeparator()
            
            # Close
            close_action = QAction("Close Tab", self)
            close_action.triggered.connect(lambda: self.close_tab(index))
            menu.addAction(close_action)
            
            # Close Others
            close_others_action = QAction("Close Other Tabs", self)
            close_others_action.triggered.connect(lambda: self.close_other_tabs(index))
            menu.addAction(close_others_action)
            
            # Close Right
            close_right_action = QAction("Close Tabs to the Right", self)
            close_right_action.triggered.connect(lambda: self.close_tabs_to_right(index))
            menu.addAction(close_right_action)
            
        menu.exec(self.mapToGlobal(position))
        

            
    def reload_tab(self, index):
        """Reload tab at index"""
        widget = self.widget(index)
        if widget and hasattr(widget, 'reload'):
            widget.reload()
            
    def close_other_tabs(self, index):
        """Close all tabs except the one at index"""
        # Iterate backwards to avoid index shifting issues
        for i in range(self.count() - 1, -1, -1):
            if i != index:
                self.close_tab(i)
                
    def close_tabs_to_right(self, index):
        """Close all tabs to the right of index"""
        for i in range(self.count() - 1, index, -1):
            self.close_tab(i)

