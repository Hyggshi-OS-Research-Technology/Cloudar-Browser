#pragma once

#include <QDialog>
#include <QListWidget>

#include "ExtensionManager.h"
#include "WebExtensionManager.h"
#include "CrxInstaller.h"

class QLabel;

// Two tabs: the original userscript list (ExtensionManager), and a second
// tab for real, manifest.json-based extensions (WebExtensionManager) —
// separate systems, separate lists, one dialog.
class ExtensionsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExtensionsDialog(ExtensionManager *manager, WebExtensionManager *webExtManager,
                               QWidget *parent = nullptr);

private slots:
    void refresh();
    void handleItemChanged(QListWidgetItem *item);
    void openFolder();

    void refreshWebExtensions();
    void handleWebExtItemChanged(QListWidgetItem *item);
    void openWebExtensionsFolder();
    void installPackageFile();
    void installPackageUrl();
    void updateInstallStatus(const QString &message);

private:
    ExtensionManager *m_manager;
    QListWidget *m_list;

    WebExtensionManager *m_webExtManager;
    QListWidget *m_webExtList;
    CrxInstaller *m_crxInstaller;
    QLabel *m_installStatus;
};
