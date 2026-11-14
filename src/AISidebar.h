#ifndef AI_SIDEBAR_H
#define AI_SIDEBAR_H

#include <QDockWidget>
#include <QTabWidget>
#include <QComboBox>
#include <QWebEngineView>
#include <QMap>

class MainWindow;

class AISidebar : public QDockWidget {
    Q_OBJECT

public:
    explicit AISidebar(MainWindow *window, QWidget *parent = nullptr);

    void toggleSidebar();
    void retranslateUi();

private slots:
    void onProviderChanged(const QString &name);
    void openService(const QString &url);

private:
    void setupChatTab();
    void setupServicesTab();

    MainWindow *m_window = nullptr;
    QTabWidget *m_tabs = nullptr;
    QComboBox *m_providerCombo = nullptr;
    QWebEngineView *m_chatView = nullptr;
    QMap<QString, QString> m_providers;
};

#endif // AI_SIDEBAR_H
