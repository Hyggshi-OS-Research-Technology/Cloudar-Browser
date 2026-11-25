#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QHash>
#include <QJsonObject>
#include <QTranslator>

// ---------------------------------------------------------------------------
// The dictionaries in lang/*.json are keyed by symbolic names
// ("tools_settings": "Settings") because that is what the Qt-free call sites use:
// get_text("tools_settings"). Qt's own translation machinery, on the other hand,
// works from the *source string* — tr("Settings") — so the two have to meet
// somewhere. This translator is that meeting point: it is handed both the
// English dictionary and the target one, builds English text -> translation, and
// then Qt's tr() calls are translated with no call site knowing about JSON.
//
// It also means a string that is missing from a dictionary degrades exactly the
// way Qt users expect: translate() returns an empty QString and tr() falls back
// to the English source text. Nothing renders blank, nothing renders a key name.
// ---------------------------------------------------------------------------
class DictionaryTranslator : public QTranslator
{
    Q_OBJECT
public:
    explicit DictionaryTranslator(QObject *parent = nullptr) : QTranslator(parent) {}

    // `target` may be empty: the translator then holds no entries and reports
    // itself as empty, which is what makes Qt skip it entirely for English.
    void setDictionaries(const QJsonObject &english, const QJsonObject &target);

    bool isEmpty() const override { return m_map.isEmpty(); }

    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;

    // Exposed for the coverage check in tools/check-i18n.js: the number of
    // English strings this dictionary can actually answer for.
    int entryCount() const { return m_map.size(); }

private:
    QHash<QString, QString> m_map;
};

class LanguageManager : public QObject {
    Q_OBJECT

public:
    static LanguageManager* instance();

    QString getText(const QString &key) const;
    QString trKey(const QString &key) const { return getText(key); }
    
    // The stored preference wins over the system locale. "system" is a real
    // choice, not an absent one: it means "follow the OS", and it is what the
    // settings page shows when the user has never picked a language.
    bool setLanguage(const QString &code);
    QString getCurrentLanguage() const;
    QMap<QString, QString> getAvailableLanguages() const;

    // "system" | a language code. Kept separate from getCurrentLanguage(),
    // which always reports the *resolved* language: a UI that shows "system"
    // as the current language would be lying about what it is displaying.
    QString preference() const;
    bool followsSystemLocale() const { return m_preference == QLatin1String("system"); }

    // Where the dictionaries actually came from, for the diagnostics line on the
    // settings page. Empty when nothing was found, which is a real state worth
    // reporting rather than silently falling back to English forever.
    QString sourceDescription() const;

    // Installs (or removes) the QTranslator for the current language on the
    // application. Must be called once after QApplication exists, and again
    // after every setLanguage() — see MainWindow, which calls it and then
    // retranslates the chrome.
    void installOnApplication();
    // Number of English strings the active dictionary can answer, and the number
    // of English strings in the reference dictionary. Shown on the settings page
    // so "translated" is a number, not a claim.
    int translatedStringCount() const;
    int referenceStringCount() const;
    // How many reference strings a given dictionary can answer for, counted the
    // same way DictionaryTranslator counts them (a value identical to the English
    // one is not a translation). Lets the settings page say "incomplete" about a
    // language it is not currently displaying.
    int stringCountFor(const QString &code) const;

signals:
    void languageChanged(const QString &code);

private:
    explicit LanguageManager(QObject *parent = nullptr);
    void loadTranslations();
    void detectSystemLanguage();
    // Reads one dictionary from the lang/ folder first (so a translation can be
    // tested without rebuilding), then from the bundle.
    bool loadDictionary(const QString &code, QJsonObject *out) const;
    void resolvePreference();

    QString m_currentLanguage;
    QString m_preference = QStringLiteral("system");
    DictionaryTranslator *m_qtTranslator = nullptr;
    mutable QString m_source;
    QJsonObject m_translations;
    QJsonObject m_fallback;
    QMap<QString, QString> m_supportedLanguages;
};

// Global helper matching Python's tr() / get_text()
inline QString get_text(const QString &key) {
    return LanguageManager::instance()->getText(key);
}

#endif // LANGUAGE_H
