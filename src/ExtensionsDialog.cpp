#include "ExtensionsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QWidget>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QInputDialog>
#include <QProgressBar>

ExtensionsDialog::ExtensionsDialog(ExtensionManager *manager, WebExtensionManager *webExtManager,
                                    QWidget *parent)
    : QDialog(parent), m_manager(manager), m_webExtManager(webExtManager)
{
    setWindowTitle("Extensions");
    resize(560, 460);

    auto *outer = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    outer->addWidget(tabs);

    // --- Tab 1: simple userscripts (ExtensionManager) ---
    auto *userscriptsPage = new QWidget(this);
    auto *layout = new QVBoxLayout(userscriptsPage);

    auto *hint = new QLabel(
        "Drop .js userscripts into the extensions folder, then toggle them here.\n"
        "Optional header: // @name My Script  // @match *", userscriptsPage);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_list = new QListWidget(userscriptsPage);
    layout->addWidget(m_list);
    connect(m_list, &QListWidget::itemChanged, this, &ExtensionsDialog::handleItemChanged);

    auto *buttonsRow = new QHBoxLayout();
    auto *openFolderBtn = new QPushButton("Open Extensions Folder", userscriptsPage);
    connect(openFolderBtn, &QPushButton::clicked, this, &ExtensionsDialog::openFolder);
    buttonsRow->addWidget(openFolderBtn);

    auto *reloadBtn = new QPushButton("Reload", userscriptsPage);
    connect(reloadBtn, &QPushButton::clicked, this, &ExtensionsDialog::refresh);
    buttonsRow->addWidget(reloadBtn);
    layout->addLayout(buttonsRow);

    tabs->addTab(userscriptsPage, "Userscripts");

    // --- Tab 2: real, manifest.json-based extensions (WebExtensionManager) ---
    auto *webExtPage = new QWidget(this);
    auto *webExtLayout = new QVBoxLayout(webExtPage);

    auto *webExtHint = new QLabel(
        "\"Load unpacked\" style: drop a folder containing manifest.json into\n"
        "the folder below, then Reload. Each extension gets its own content\n"
        "scripts, background page, storage, and permissions.", webExtPage);
    webExtHint->setWordWrap(true);
    webExtLayout->addWidget(webExtHint);

    m_webExtList = new QListWidget(webExtPage);
    webExtLayout->addWidget(m_webExtList);
    connect(m_webExtList, &QListWidget::itemChanged, this, &ExtensionsDialog::handleWebExtItemChanged);

    auto *webExtButtonsRow = new QHBoxLayout();
    auto *openWebExtFolderBtn = new QPushButton("Open Extensions Folder", webExtPage);
    connect(openWebExtFolderBtn, &QPushButton::clicked, this, &ExtensionsDialog::openWebExtensionsFolder);
    webExtButtonsRow->addWidget(openWebExtFolderBtn);

    auto *webExtReloadBtn = new QPushButton("Reload", webExtPage);
    connect(webExtReloadBtn, &QPushButton::clicked, this, &ExtensionsDialog::refreshWebExtensions);
    webExtButtonsRow->addWidget(webExtReloadBtn);
    webExtLayout->addLayout(webExtButtonsRow);

    auto *installRow = new QHBoxLayout();
    auto *installFileBtn = new QPushButton("Install CRX/ZIP File…", webExtPage);
    connect(installFileBtn, &QPushButton::clicked, this, &ExtensionsDialog::installPackageFile);
    installRow->addWidget(installFileBtn);
    auto *installUrlBtn = new QPushButton("Install from URL…", webExtPage);
    connect(installUrlBtn, &QPushButton::clicked, this, &ExtensionsDialog::installPackageUrl);
    installRow->addWidget(installUrlBtn);
    webExtLayout->addLayout(installRow);

    m_installStatus = new QLabel(webExtPage);
    m_installStatus->setWordWrap(true);
    webExtLayout->addWidget(m_installStatus);

    tabs->addTab(webExtPage, "Extensions (manifest.json)");

    m_crxInstaller = new CrxInstaller(m_webExtManager ? m_webExtManager->extensionsRootDir() : QString(), this);
    connect(m_crxInstaller, &CrxInstaller::statusChanged, this, &ExtensionsDialog::updateInstallStatus);
    connect(m_crxInstaller, &CrxInstaller::progress, this, [this](qint64 received, qint64 total) {
        const QString suffix = total > 0
            ? QString(" (%1 / %2 MiB)").arg(received / (1024.0 * 1024.0), 0, 'f', 1).arg(total / (1024.0 * 1024.0), 0, 'f', 1)
            : QString(" (%1 MiB)").arg(received / (1024.0 * 1024.0), 0, 'f', 1);
        updateInstallStatus("Downloading…" + suffix);
    });
    connect(m_crxInstaller, &CrxInstaller::installed, this, [this](const QString &) {
        refreshWebExtensions();
    });
    connect(m_crxInstaller, &CrxInstaller::failed, this, [this](const QString &message) {
        updateInstallStatus("Install failed: " + message);
    });

    refresh();
    refreshWebExtensions();
}

void ExtensionsDialog::refresh()
{
    m_manager->reload();

    m_list->blockSignals(true);
    m_list->clear();
    for (const UserScriptInfo &info : m_manager->discover()) {
        auto *item = new QListWidgetItem(QString("%1  (%2)").arg(info.name, info.match));
        item->setData(Qt::UserRole, info.fileName);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(info.enabled ? Qt::Checked : Qt::Unchecked);
        m_list->addItem(item);
    }
    m_list->blockSignals(false);

    if (m_list->count() == 0) {
        auto *item = new QListWidgetItem("No userscripts found — drop a .js file into the folder above.");
        item->setFlags(Qt::NoItemFlags);
        m_list->addItem(item);
    }
}

void ExtensionsDialog::handleItemChanged(QListWidgetItem *item)
{
    const QString fileName = item->data(Qt::UserRole).toString();
    if (fileName.isEmpty())
        return;
    m_manager->setEnabled(fileName, item->checkState() == Qt::Checked);
}

void ExtensionsDialog::openFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_manager->extensionsDirPath()));
}

void ExtensionsDialog::refreshWebExtensions()
{
    if (!m_webExtManager)
        return;

    m_webExtManager->reload();

    m_webExtList->blockSignals(true);
    m_webExtList->clear();
    for (const LoadedWebExtension &ext : m_webExtManager->extensions()) {
        QString label = QString("%1  v%2").arg(ext.manifest.name, ext.manifest.version);
        if (!ext.manifest.description.isEmpty())
            label += "\n" + ext.manifest.description;
        if (!ext.manifest.permissions.isEmpty())
            label += "\nPermissions: " + ext.manifest.permissions.join(", ");

        auto *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, ext.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(ext.enabled ? Qt::Checked : Qt::Unchecked);
        m_webExtList->addItem(item);
    }
    m_webExtList->blockSignals(false);

    if (m_webExtList->count() == 0) {
        auto *item = new QListWidgetItem(
            "No extensions found — drop a folder with a manifest.json into the folder above.");
        item->setFlags(Qt::NoItemFlags);
        m_webExtList->addItem(item);
    }
}

void ExtensionsDialog::handleWebExtItemChanged(QListWidgetItem *item)
{
    const QString extensionId = item->data(Qt::UserRole).toString();
    if (extensionId.isEmpty() || !m_webExtManager)
        return;
    // setEnabled() calls reload() internally, which rebuilds the whole
    // extensions list (including background pages) — refresh the widget
    // afterward so checkbox state stays in sync with what actually loaded.
    m_webExtManager->setEnabled(extensionId, item->checkState() == Qt::Checked);
    refreshWebExtensions();
}

void ExtensionsDialog::openWebExtensionsFolder()
{
    if (!m_webExtManager)
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_webExtManager->extensionsRootDir()));
}

void ExtensionsDialog::installPackageFile()
{
    const QString file = QFileDialog::getOpenFileName(this, "Install Extension Package", QString(),
                                                       "Extension packages (*.crx *.zip);;All files (*)");
    if (!file.isEmpty())
        m_crxInstaller->installFile(file);
}

void ExtensionsDialog::installPackageUrl()
{
    bool accepted = false;
    const QString input = QInputDialog::getText(this, "Install Extension from URL", "CRX or ZIP URL:",
                                                 QLineEdit::Normal, QString(), &accepted);
    if (!accepted || input.trimmed().isEmpty())
        return;
    m_crxInstaller->downloadAndInstall(QUrl::fromUserInput(input.trimmed()));
}

void ExtensionsDialog::updateInstallStatus(const QString &message)
{
    if (m_installStatus)
        m_installStatus->setText(message);
}
