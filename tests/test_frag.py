import sys
from PyQt6.QtCore import QUrl, QByteArray, QBuffer, QTimer
from PyQt6.QtWidgets import QApplication
from PyQt6.QtWebEngineCore import QWebEngineUrlSchemeHandler, QWebEngineUrlRequestJob, QWebEngineProfile, QWebEngineUrlScheme
from PyQt6.QtWebEngineWidgets import QWebEngineView

class Handler(QWebEngineUrlSchemeHandler):
    def requestStarted(self, job: QWebEngineUrlRequestJob):
        data = b"""
        <html>
        <body>
            <script>
                document.title = "HASH: " + window.location.hash;
            </script>
        </body>
        </html>
        """
        buf = QBuffer(job)
        buf.setData(data)
        job.reply(b"text/html", buf)

scheme = QWebEngineUrlScheme(b"test")
scheme.setSyntax(QWebEngineUrlScheme.Syntax.HostAndPort)
QWebEngineUrlScheme.registerScheme(scheme)

app = QApplication(sys.argv)
profile = QWebEngineProfile.defaultProfile()
handler = Handler()
profile.installUrlSchemeHandler(b"test", handler)

view = QWebEngineView()
view.setUrl(QUrl("test://host/path#language"))
view.show()

def check_title():
    print("Title:", view.title())
    sys.exit(0)

QTimer.singleShot(1500, check_title)
app.exec()
