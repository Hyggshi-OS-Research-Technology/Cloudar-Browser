#include "AISidebar.h"
#include "MainWindow.h"
#include "Language.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>

AISidebar::AISidebar(MainWindow *window, QWidget *parent)
    : QDockWidget(get_text("ai_sidebar_title"), parent), m_window(window) {
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);

    m_providers = {
        {"ChatGPT", "https://chatgpt.com/"},
        {"Gemini", "https://gemini.google.com/"},
        {"Claude", "https://claude.ai/"},
        {"DeepSeek", "https://chat.deepseek.com/"},
        {"Copilot", "https://copilot.microsoft.com/"},
        {"Perplexity", "https://www.perplexity.ai/"},
        {"Blackbox", "https://www.blackbox.ai/"},
        {"HuggingChat", "https://huggingface.co/chat/"},
        {"Poe", "https://poe.com/"}
    };

    QWidget *container = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_tabs = new QTabWidget(container);
    m_tabs->setDocumentMode(true);

    setupChatTab();
    setupServicesTab();

    mainLayout->addWidget(m_tabs);
    setWidget(container);
}

void AISidebar::setupChatTab() {
    QWidget *chatWidget = new QWidget(m_tabs);
    QVBoxLayout *chatLayout = new QVBoxLayout(chatWidget);
    chatLayout->setContentsMargins(8, 8, 8, 8);
    chatLayout->setSpacing(8);

    QHBoxLayout *topBar = new QHBoxLayout();
    QLabel *lbl = new QLabel("AI:", chatWidget);
    m_providerCombo = new QComboBox(chatWidget);

    for (auto it = m_providers.begin(); it != m_providers.end(); ++it) {
        m_providerCombo->addItem(it.key());
    }

    connect(m_providerCombo, &QComboBox::currentTextChanged, this, &AISidebar::onProviderChanged);

    topBar->addWidget(lbl);
    topBar->addWidget(m_providerCombo, 1);
    chatLayout->addLayout(topBar);

    m_chatView = new QWebEngineView(chatWidget);
    m_chatView->setUrl(QUrl("https://chatgpt.com/"));
    chatLayout->addWidget(m_chatView, 1);

    m_tabs->addTab(chatWidget, get_text("ai_chat_tab"));
}

void AISidebar::setupServicesTab() {
    QScrollArea *scroll = new QScrollArea(m_tabs);
    scroll->setWidgetResizable(true);

    QWidget *servicesWidget = new QWidget(scroll);
    QGridLayout *grid = new QGridLayout(servicesWidget);
    grid->setContentsMargins(12, 12, 12, 12);
    grid->setSpacing(10);

    struct ServiceItem {
        QString icon;
        QString name;
        QString url;
    };

    QList<ServiceItem> services = {
        {"G", "Google", "https://www.google.com/"},
        {"M", "Gmail", "https://mail.google.com/"},
        {"D", "Drive", "https://drive.google.com/"},
        {QString::fromUtf8("\u25b6"), "YouTube", "https://www.youtube.com/"},
        {QString::fromUtf8("\u2316"), "Maps", "https://www.google.com/maps"},
        {QString::fromUtf8("\u25eb"), "Photos", "https://photos.google.com/"},
        {QString::fromUtf8("\u2261"), "Docs", "https://docs.google.com/"},
        {QString::fromUtf8("\u25a3"), "Calendar", "https://calendar.google.com/"},
        {QString::fromUtf8("\u6587"), "Translate", "https://translate.google.com/"},
        {QString::fromUtf8("\u2713"), "Keep", "https://keep.google.com/"}
    };

    int row = 0;
    int col = 0;
    for (const auto &s : services) {
        QPushButton *btn = new QPushButton(QString("%1  %2").arg(s.icon, s.name), servicesWidget);
        btn->setMinimumHeight(44);
        QString u = s.url;
        connect(btn, &QPushButton::clicked, [this, u]() {
            openService(u);
        });
        grid->addWidget(btn, row, col);
        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }

    scroll->setWidget(servicesWidget);
    m_tabs->addTab(scroll, get_text("ai_services_tab"));
}

void AISidebar::onProviderChanged(const QString &name) {
    if (m_providers.contains(name) && m_chatView) {
        m_chatView->setUrl(QUrl(m_providers.value(name)));
    }
}

void AISidebar::openService(const QString &url) {
    if (m_window) {
        m_window->openUrl(QUrl(url));
    }
}

void AISidebar::toggleSidebar() {
    setVisible(!isVisible());
}

void AISidebar::retranslateUi() {
    setWindowTitle(get_text("ai_sidebar_title"));
    // Only the two tab labels are ours; the provider names below them are the
    // services' own names and stay as they are.
    if (m_tabs && m_tabs->count() >= 1)
        m_tabs->setTabText(0, get_text("ai_chat_tab"));
    if (m_tabs && m_tabs->count() >= 2)
        m_tabs->setTabText(1, get_text("ai_services_tab"));
}
