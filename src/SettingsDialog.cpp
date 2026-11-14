#include "SettingsDialog.h"
#include "ThemeManager.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QFileDialog>
#include <QColorDialog>
#include <QStandardPaths>
#include <QDir>

namespace {
const char *kHomeKey = "browser/homePage";
const char *kSearchKey = "browser/searchEngine";
const char *kCookiesKey = "browser/persistCookies";
const char *kDownloadDirKey = "browser/downloadDir";
const char *kAskLocationKey = "browser/askDownloadLocation";

QString defaultDownloadDir()
{
    const QString standard = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return standard.isEmpty() ? (QDir::homePath() + "/Downloads") : standard;
}
}

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Settings");

    auto *layout = new QFormLayout(this);

    m_homeEdit = new QLineEdit(this);
    layout->addRow("Home page:", m_homeEdit);

    m_searchCombo = new QComboBox(this);
    m_searchCombo->addItem("DuckDuckGo", "https://duckduckgo.com/?q=%s");
    m_searchCombo->addItem("Google", "https://www.google.com/search?q=%s");
    m_searchCombo->addItem("Bing", "https://www.bing.com/search?q=%s");
    layout->addRow("Search engine:", m_searchCombo);

    m_cookiesCheck = new QCheckBox("Keep cookies between sessions", this);
    layout->addRow(m_cookiesCheck);

    // --- Downloads ---
    auto *downloadRow = new QWidget(this);
    auto *downloadRowLayout = new QHBoxLayout(downloadRow);
    downloadRowLayout->setContentsMargins(0, 0, 0, 0);
    m_downloadDirEdit = new QLineEdit(downloadRow);
    auto *browseBtn = new QPushButton("Browse…", downloadRow);
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::browseDownloadsDir);
    downloadRowLayout->addWidget(m_downloadDirEdit);
    downloadRowLayout->addWidget(browseBtn);
    layout->addRow("Downloads folder:", downloadRow);

    m_askLocationCheck = new QCheckBox("Ask where to save each file before downloading", this);
    layout->addRow(m_askLocationCheck);

    // --- Appearance ---
    m_accentColorBtn = new QPushButton(this);
    connect(m_accentColorBtn, &QPushButton::clicked, this, &SettingsDialog::pickAccentColor);
    layout->addRow("Accent color:", m_accentColorBtn);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);

    load();
}

void SettingsDialog::load()
{
    QSettings settings;
    m_homeEdit->setText(settings.value(kHomeKey, "cloudar://newtab").toString());

    const QString engine = settings.value(kSearchKey, "https://duckduckgo.com/?q=%s").toString();
    int idx = m_searchCombo->findData(engine);
    m_searchCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    m_cookiesCheck->setChecked(settings.value(kCookiesKey, true).toBool());

    m_downloadDirEdit->setText(settings.value(kDownloadDirKey, defaultDownloadDir()).toString());
    m_askLocationCheck->setChecked(settings.value(kAskLocationKey, true).toBool());

    m_accentColorValue = ThemeManager::instance().accentColor();
    updateAccentButtonSwatch();
}

void SettingsDialog::updateAccentButtonSwatch()
{
    m_accentColorBtn->setText(m_accentColorValue.name());
    m_accentColorBtn->setStyleSheet(QString(
        "background: %1; color: %2; border-radius: 4px; padding: 4px 10px;")
        .arg(m_accentColorValue.name(), ThemeManager::readableTextOn(m_accentColorValue).name()));
}

void SettingsDialog::browseDownloadsDir()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Choose downloads folder", m_downloadDirEdit->text());
    if (!dir.isEmpty())
        m_downloadDirEdit->setText(dir);
}

void SettingsDialog::pickAccentColor()
{
    const QColor chosen = QColorDialog::getColor(m_accentColorValue, this, "Choose accent color");
    if (chosen.isValid()) {
        m_accentColorValue = chosen;
        updateAccentButtonSwatch();
    }
}

void SettingsDialog::save()
{
    QSettings settings;
    settings.setValue(kHomeKey, m_homeEdit->text());
    settings.setValue(kSearchKey, m_searchCombo->currentData().toString());
    settings.setValue(kCookiesKey, m_cookiesCheck->isChecked());

    QString downloadDir = m_downloadDirEdit->text().trimmed();
    if (downloadDir.isEmpty())
        downloadDir = defaultDownloadDir();
    settings.setValue(kDownloadDirKey, downloadDir);
    settings.setValue(kAskLocationKey, m_askLocationCheck->isChecked());

    // Applies (and persists) app-wide immediately — every widget listening
    // to ThemeManager::accentChanged, including the title bar, repaints.
    ThemeManager::instance().setAccentColor(m_accentColorValue);

    accept();
}

QString SettingsDialog::homePage() const
{
    QSettings settings;
    return settings.value(kHomeKey, "cloudar://newtab").toString();
}

QString SettingsDialog::searchEngineTemplate() const
{
    QSettings settings;
    return settings.value(kSearchKey, "https://duckduckgo.com/?q=%s").toString();
}

bool SettingsDialog::persistCookies() const
{
    QSettings settings;
    return settings.value(kCookiesKey, true).toBool();
}

QString SettingsDialog::downloadsDir() const
{
    QSettings settings;
    return settings.value(kDownloadDirKey, defaultDownloadDir()).toString();
}

bool SettingsDialog::askDownloadLocation() const
{
    QSettings settings;
    return settings.value(kAskLocationKey, true).toBool();
}
