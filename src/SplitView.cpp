#include "SplitView.h"
#include "BrowserResources.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QWebEnginePage>

SplitPane::SplitPane(QWebEngineProfile *profile, const QUrl &url, QWidget *parent)
    : QWidget(parent) {
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QFrame *bar = new QFrame(this);
    bar->setObjectName("SplitPaneBar");
    bar->setFixedHeight(42);
    QHBoxLayout *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(8, 5, 8, 5);
    barLayout->setSpacing(6);

    m_backBtn = new QToolButton(bar);
    m_backBtn->setObjectName("SplitPaneButton");
    m_backBtn->setText(BrowserResources::getIcon("back"));
    m_backBtn->setFixedSize(28, 28);

    m_forwardBtn = new QToolButton(bar);
    m_forwardBtn->setObjectName("SplitPaneButton");
    m_forwardBtn->setText(BrowserResources::getIcon("forward"));
    m_forwardBtn->setFixedSize(28, 28);

    m_reloadBtn = new QToolButton(bar);
    m_reloadBtn->setObjectName("SplitPaneButton");
    m_reloadBtn->setText(BrowserResources::getIcon("reload"));
    m_reloadBtn->setFixedSize(28, 28);

    m_urlBar = new QLineEdit(bar);
    m_urlBar->setObjectName("SplitPaneUrlBar");
    m_urlBar->setPlaceholderText("Type URL or search");

    barLayout->addWidget(m_backBtn);
    barLayout->addWidget(m_forwardBtn);
    barLayout->addWidget(m_reloadBtn);
    barLayout->addWidget(m_urlBar, 1);

    m_webView = new QWebEngineView(this);
    if (profile) {
        auto *page = new QWebEnginePage(profile, m_webView);
        m_webView->setPage(page);
    }
    if (url.isValid()) {
        m_webView->setUrl(url);
    } else {
        m_webView->setUrl(QUrl("cloudar://newtab"));
    }

    rootLayout->addWidget(bar);
    rootLayout->addWidget(m_webView, 1);

    connect(m_backBtn, &QToolButton::clicked, m_webView, &QWebEngineView::back);
    connect(m_forwardBtn, &QToolButton::clicked, m_webView, &QWebEngineView::forward);
    connect(m_reloadBtn, &QToolButton::clicked, m_webView, &QWebEngineView::reload);
    connect(m_urlBar, &QLineEdit::returnPressed, this, &SplitPane::onReturnPressed);
    connect(m_webView, &QWebEngineView::urlChanged, this, &SplitPane::onUrlChanged);
}

void SplitPane::onUrlChanged(const QUrl &url) {
    if (url.scheme() == "cloudar" && url.host() == "newtab") {
        m_urlBar->clear();
    } else {
        m_urlBar->setText(url.toString());
    }
}

void SplitPane::onReturnPressed() {
    navigateToUrl(m_urlBar->text().trimmed());
}

void SplitPane::navigateToUrl(const QString &text) {
    if (text.isEmpty()) return;
    QUrl url = QUrl::fromUserInput(text);
    if (!text.contains('.') && !text.contains("://") && !text.startsWith("localhost")) {
        url = QUrl(QString("https://www.google.com/search?q=%1").arg(QUrl::toPercentEncoding(text)));
    }
    m_webView->setUrl(url);
}

SplitView::SplitView(QWebEngineProfile *profile, const QUrl &url1, const QUrl &url2, QWidget *parent)
    : QWidget(parent) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName("SplitBrowserSplitter");

    m_leftPane = new SplitPane(profile, url1, m_splitter);
    m_rightPane = new SplitPane(profile, url2, m_splitter);

    m_splitter->addWidget(m_leftPane);
    m_splitter->addWidget(m_rightPane);
    m_splitter->setSizes({500, 500});

    layout->addWidget(m_splitter);
}
