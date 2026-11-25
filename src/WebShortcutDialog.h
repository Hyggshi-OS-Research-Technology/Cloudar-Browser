#pragma once

#include <QDialog>
#include <QString>
#include <QUrl>

class QLineEdit;

// ---------------------------------------------------------------------------
// "Create web shortcut": name + the page it points at.
//
// Deliberately two fields and a sentence. There is no icon picker, no window
// size choice and no "open at login": those would imply an app-view install
// (manifest, service worker, launcher entry) that this build does not do. See
// WebShortcutStore.h for the full list of what a shortcut is not.
// ---------------------------------------------------------------------------
class WebShortcutDialog : public QDialog
{
    Q_OBJECT
public:
    // `name` prefills the field; empty means "use the host".
    WebShortcutDialog(const QString &name, const QUrl &url, QWidget *parent = nullptr);

    // The trimmed name, or the host when the field was left empty.
    QString shortcutName() const;

private:
    QUrl m_url;
    QLineEdit *m_nameEdit = nullptr;
};
