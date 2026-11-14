#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColor>

// Settings surface: home page, default search engine, cookie persistence,
// downloads (folder + whether to prompt per file), and the app's accent
// color (applied everywhere via ThemeManager, including the custom title
// bar). Backed by QSettings (INI on Linux), so there's no extra database
// dependency.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    QString homePage() const;
    QString searchEngineTemplate() const; // "%s" placeholder for the query
    bool persistCookies() const;
    QString downloadsDir() const;
    bool askDownloadLocation() const;

private slots:
    void save();
    void browseDownloadsDir();
    void pickAccentColor();

private:
    QLineEdit *m_homeEdit;
    QComboBox *m_searchCombo;
    QCheckBox *m_cookiesCheck;
    QLineEdit *m_downloadDirEdit;
    QCheckBox *m_askLocationCheck;
    QPushButton *m_accentColorBtn;
    QColor m_accentColorValue;

    void load();
    void updateAccentButtonSwatch();
};
