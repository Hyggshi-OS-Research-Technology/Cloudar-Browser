#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QJsonObject>

class LanguageManager : public QObject {
    Q_OBJECT

public:
    static LanguageManager* instance();

    QString getText(const QString &key) const;
    QString trKey(const QString &key) const { return getText(key); }
    
    bool setLanguage(const QString &code);
    QString getCurrentLanguage() const;
    QMap<QString, QString> getAvailableLanguages() const;

signals:
    void languageChanged(const QString &code);

private:
    explicit LanguageManager(QObject *parent = nullptr);
    void loadTranslations();
    void detectSystemLanguage();

    QString m_currentLanguage;
    QJsonObject m_translations;
    QJsonObject m_fallback;
    QMap<QString, QString> m_supportedLanguages;
};

// Global helper matching Python's tr() / get_text()
inline QString get_text(const QString &key) {
    return LanguageManager::instance()->getText(key);
}

#endif // LANGUAGE_H
