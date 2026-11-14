#ifndef SPLIT_VIEW_H
#define SPLIT_VIEW_H

#include <QWidget>
#include <QSplitter>
#include <QLineEdit>
#include <QToolButton>
#include <QUrl>
#include <QWebEngineView>
#include <QWebEngineProfile>

class SplitPane : public QWidget {
    Q_OBJECT

public:
    explicit SplitPane(QWebEngineProfile *profile = nullptr, const QUrl &url = QUrl(), QWidget *parent = nullptr);

    QWebEngineView* getWebView() const { return m_webView; }
    void navigateToUrl(const QString &text);

private slots:
    void onUrlChanged(const QUrl &url);
    void onReturnPressed();

private:
    QToolButton *m_backBtn = nullptr;
    QToolButton *m_forwardBtn = nullptr;
    QToolButton *m_reloadBtn = nullptr;
    QLineEdit *m_urlBar = nullptr;
    QWebEngineView *m_webView = nullptr;
};

class SplitView : public QWidget {
    Q_OBJECT

public:
    explicit SplitView(QWebEngineProfile *profile = nullptr, const QUrl &url1 = QUrl(), const QUrl &url2 = QUrl(), QWidget *parent = nullptr);

    SplitPane* leftPane() const { return m_leftPane; }
    SplitPane* rightPane() const { return m_rightPane; }

private:
    QSplitter *m_splitter = nullptr;
    SplitPane *m_leftPane = nullptr;
    SplitPane *m_rightPane = nullptr;
};

#endif // SPLIT_VIEW_H
