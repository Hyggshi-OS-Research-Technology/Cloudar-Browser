#include "WebShortcutDialog.h"

#include "Language.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

WebShortcutDialog::WebShortcutDialog(const QString &name, const QUrl &url, QWidget *parent)
    : QDialog(parent), m_url(url)
{
    setWindowTitle(get_text("shortcut_dialog_title"));
    setModal(true);
    resize(420, 0);

    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(get_text("shortcut_dialog_intro"), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *form = new QFormLayout();
    m_nameEdit = new QLineEdit(name.isEmpty() ? url.host() : name, this);
    m_nameEdit->setPlaceholderText(url.host());
    m_nameEdit->selectAll();
    form->addRow(get_text("shortcut_name_label"), m_nameEdit);

    // The URL is shown but not editable: this dialog is reached from a page the
    // user is already looking at, and an editable URL turns it into "create a
    // bookmark in a worse place". Changing the site means navigating there.
    auto *urlLabel = new QLabel(url.toDisplayString(), this);
    urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    urlLabel->setWordWrap(true);
    form->addRow(get_text("shortcut_url_label"), urlLabel);
    layout->addLayout(form);

    auto *note = new QLabel(get_text("shortcut_dialog_note"), this);
    note->setWordWrap(true);
    // The honest limitation, in the dialog rather than only in the docs: this is
    // not an install, so it cannot work offline.
    note->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(note);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(get_text("shortcut_create"));
    // Cancel keeps the platform's own label: it is a word every Qt dialog in
    // this application already uses from Qt's own translations, and inventing a
    // second source for it would make the two disagree in some languages.

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // Creating is the default action; an empty name falls back to the host in
    // shortcutName() rather than refusing, because a nameless window is better
    // than an error over a field the user may not care about.
    buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    m_nameEdit->setFocus();
}

QString WebShortcutDialog::shortcutName() const
{
    const QString trimmed = m_nameEdit->text().trimmed();
    return trimmed.isEmpty() ? m_url.host() : trimmed;
}
