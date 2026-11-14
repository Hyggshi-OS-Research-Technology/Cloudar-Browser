#include "Language.h"
#include "BrowserResources.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QDebug>

LanguageManager* LanguageManager::instance() {
    static LanguageManager s_instance;
    return &s_instance;
}

LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent), m_currentLanguage("en") {
    m_supportedLanguages = {
        {"en", "English"},
        {"vi", "Tiếng Việt"},
        {"ko", "한국어"},
        {"fr", "Français"},
        {"de", "Deutsch"},
        {"es", "Español"},
        {"ru", "Русский"},
        {"pt", "Português"},
        {"ja", "日本語"},
        {"zh", "简体中文"}
    };

    detectSystemLanguage();
    loadTranslations();
}

void LanguageManager::detectSystemLanguage() {
    QString sysLocale = QLocale::system().name().toLower();
    QString langPrefix = sysLocale.split('_').first();

    if (m_supportedLanguages.contains(langPrefix)) {
        m_currentLanguage = langPrefix;
    } else {
        m_currentLanguage = "en";
    }
}

void LanguageManager::loadTranslations() {
    QString langDir = BrowserResources::getAppRootDir() + "/lang";

    // Load fallback English
    QString enPath = langDir + "/en.json";
    m_fallback = BrowserResources::loadJsonObject(enPath);

    // Load current language
    if (m_currentLanguage != "en") {
        QString curPath = langDir + "/" + m_currentLanguage + ".json";
        m_translations = BrowserResources::loadJsonObject(curPath);
    } else {
        m_translations = m_fallback;
    }
}

bool LanguageManager::setLanguage(const QString &code) {
    QString lowerCode = code.toLower().trimmed();
    if (!m_supportedLanguages.contains(lowerCode)) {
        lowerCode = "en";
    }

    if (m_currentLanguage == lowerCode && !m_translations.isEmpty()) {
        return true;
    }

    m_currentLanguage = lowerCode;
    QString langDir = BrowserResources::getAppRootDir() + "/lang";
    QString curPath = langDir + "/" + m_currentLanguage + ".json";
    m_translations = BrowserResources::loadJsonObject(curPath);

    emit languageChanged(m_currentLanguage);
    return true;
}

QString LanguageManager::getText(const QString &key) const {
    if (key.isEmpty()) return "";

    if (m_translations.contains(key)) {
        return m_translations.value(key).toString();
    }
    if (m_fallback.contains(key)) {
        return m_fallback.value(key).toString();
    }
    return key;
}

QString LanguageManager::getCurrentLanguage() const {
    return m_currentLanguage;
}

QMap<QString, QString> LanguageManager::getAvailableLanguages() const {
    return m_supportedLanguages;
}
