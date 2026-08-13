"""
Split screen view for Cloudar Browser.
Provides a side-by-side browsing layout using QSplitter.
"""
from core.browser_qt import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QSplitter,
    QToolButton,
    QUrl,
    QVBoxLayout,
    QWidget,
    Qt,
    pyqtSignal,
)
from core.web_view import WebView


class SplitPane(QWidget):
    """One pane inside the split view."""

    def __init__(self, profile=None, url: QUrl = None, parent=None):
        super().__init__(parent)
        self._profile = profile

        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        bar = QFrame()
        bar.setObjectName("SplitPaneBar")
        bar.setFixedHeight(42)
        bar_layout = QHBoxLayout(bar)
        bar_layout.setContentsMargins(8, 5, 8, 5)
        bar_layout.setSpacing(6)

        self.back_btn = QToolButton()
        self.back_btn.setObjectName("SplitPaneButton")
        self.back_btn.setText("\u2190")
        self.back_btn.setFixedSize(28, 28)

        self.fwd_btn = QToolButton()
        self.fwd_btn.setObjectName("SplitPaneButton")
        self.fwd_btn.setText("\u2192")
        self.fwd_btn.setFixedSize(28, 28)

        self.reload_btn = QToolButton()
        self.reload_btn.setObjectName("SplitPaneButton")
        self.reload_btn.setText("\u21bb")
        self.reload_btn.setFixedSize(28, 28)

        self.url_bar = QLineEdit()
        self.url_bar.setObjectName("SplitPaneUrlBar")
        self.url_bar.setPlaceholderText("Type URL or search")

        bar_layout.addWidget(self.back_btn)
        bar_layout.addWidget(self.fwd_btn)
        bar_layout.addWidget(self.reload_btn)
        bar_layout.addWidget(self.url_bar, 1)
        root.addWidget(bar)

        self.view = WebView(profile=self._profile, isolated=True, parent=self)
        root.addWidget(self.view)

        self.back_btn.clicked.connect(self.view.back)
        self.fwd_btn.clicked.connect(self.view.forward)
        self.reload_btn.clicked.connect(self.view.reload)
        self.url_bar.returnPressed.connect(self._navigate)
        self.view.urlChanged.connect(self._on_url_changed)

        if url and not url.isEmpty():
            self.view.setUrl(url)
        else:
            self.view.setUrl(QUrl("cloudar://newtab"))

    def _navigate(self):
        text = self.url_bar.text().strip()
        if not text:
            return

        if "://" in text:
            qurl = QUrl(text)
        elif "." in text and " " not in text:
            qurl = QUrl("https://" + text)
        else:
            qurl = QUrl(f"https://www.google.com/search?q={text.replace(' ', '+')}")
        self.view.setUrl(qurl)

    def _on_url_changed(self, url: QUrl):
        self.url_bar.setText(url.toString())
        self.url_bar.setCursorPosition(0)


class SplitBrowserView(QWidget):
    """Split browser view with two independent panes."""

    urlChanged = pyqtSignal(QUrl)
    loadFinished = pyqtSignal(bool)

    def __init__(self, profile=None, left_url: QUrl = None, right_url: QUrl = None, parent=None):
        super().__init__(parent)
        self._profile = profile
        self._orientation = Qt.Orientation.Horizontal

        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        header = QFrame()
        header.setObjectName("SplitHeader")
        header.setFixedHeight(36)
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(10, 4, 10, 4)
        header_layout.setSpacing(8)

        title = QLabel("\u29c9 Split Screen")
        title.setObjectName("SplitHeaderTitle")

        self.toggle_btn = QToolButton()
        self.toggle_btn.setObjectName("SplitHeaderButton")
        self.toggle_btn.setText("\u21c4 Rotate")
        self.toggle_btn.setToolTip("Toggle horizontal or vertical split")
        self.toggle_btn.clicked.connect(self.toggle_orientation)

        header_layout.addWidget(title)
        header_layout.addStretch()
        header_layout.addWidget(self.toggle_btn)
        root.addWidget(header)

        self.splitter = QSplitter(self._orientation)
        self.splitter.setObjectName("SplitBrowserSplitter")
        self.splitter.setHandleWidth(4)
        self.splitter.setChildrenCollapsible(False)

        self.left_pane = SplitPane(profile=profile, url=left_url, parent=self.splitter)
        self.right_pane = SplitPane(profile=profile, url=right_url, parent=self.splitter)

        self.splitter.addWidget(self.left_pane)
        self.splitter.addWidget(self.right_pane)
        self.splitter.setSizes([1, 1])
        root.addWidget(self.splitter)

        self.left_pane.view.urlChanged.connect(self.urlChanged)
        self.left_pane.view.loadFinished.connect(self.loadFinished)

    def toggle_orientation(self):
        """Swap between horizontal and vertical split."""
        if self._orientation == Qt.Orientation.Horizontal:
            self._orientation = Qt.Orientation.Vertical
            self.toggle_btn.setText("\u21c5 Rotate")
        else:
            self._orientation = Qt.Orientation.Horizontal
            self.toggle_btn.setText("\u21c4 Rotate")
        self.splitter.setOrientation(self._orientation)

    def url(self):
        return self.left_pane.view.url()

    def page(self):
        return self.left_pane.view.page()

    def setUrl(self, url):
        self.left_pane.view.setUrl(url)

    def load(self, url):
        self.left_pane.view.load(url)

    def reload(self):
        self.left_pane.view.reload()

    def back(self):
        self.left_pane.view.back()

    def forward(self):
        self.left_pane.view.forward()

    def stop(self):
        self.left_pane.view.stop()

    def zoomFactor(self):
        return self.left_pane.view.zoomFactor()

    def setZoomFactor(self, factor):
        self.left_pane.view.setZoomFactor(factor)
        self.right_pane.view.setZoomFactor(factor)

    def findText(self, text, flags=0, callback=None):
        self.left_pane.view.findText(text, flags, callback)
