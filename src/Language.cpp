#include "Language.h"
#include "BrowserResources.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QLocale>
#include <QSettings>
#include <QDebug>

using BrowserResources::getAppRootDir;

void DictionaryTranslator::setDictionaries(const QJsonObject &english, const QJsonObject &target)
{
    m_map.clear();
    // English -> target, keyed by the *English text*, because that is what Qt
    // passes to translate() as sourceText. Keys whose English value is empty are
    // skipped: tr("") is never called, and an empty source string would make the
    // entry unreachable.
    for (auto it = english.constBegin(); it != english.constEnd(); ++it) {
        const QString englishText = it.value().toString();
        if (englishText.isEmpty())
            continue;
        const QString translated = target.value(it.key()).toString();
        if (translated.isEmpty() || translated == englishText)
            continue;
        // Menu entries carry a Qt accelerator marker ("&File") that the
        // dictionary value does not, so the lookup key is stored without it and
        // translate() strips it from the source text before looking up.
        m_map.insert(QString(englishText).remove(QLatin1Char('&')),
                     QString(translated).remove(QLatin1Char('&')));
    }
}

QString DictionaryTranslator::translate(const char *context, const char *sourceText,
                                        const char *disambiguation, int n) const
{
    Q_UNUSED(context);
    Q_UNUSED(disambiguation);
    Q_UNUSED(n);
    if (!sourceText)
        return QString();
    const QString source = QString::fromUtf8(sourceText);

    // Returning an empty QString is how a QTranslator says "I do not know this
    // one": QCoreApplication then tries the next translator and finally falls
    // back to the source string. Returning sourceText verbatim would instead
    // *stop* the chain and hide a later translator's entry.
    const QString translated = m_map.value(QString(source).remove(QLatin1Char('&')));
    if (translated.isEmpty())
        return QString();

    // Re-attach the accelerator: a menu entry whose source text was "&File" still
    // has to open with Alt+F, so the marker is put back in front of the
    // translated word rather than dropped.
    if (source.startsWith(QLatin1Char('&')) && !source.startsWith(QLatin1String("&&"))
        && !translated.startsWith(QLatin1Char('&'))) {
        return QLatin1Char('&') + translated;
    }
    return translated;
}

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

    m_qtTranslator = new DictionaryTranslator(this);

    resolvePreference();
    loadTranslations();
}

void LanguageManager::installOnApplication()
{
    if (!QCoreApplication::instance())
        return;
    // Remove first: installing the same translator object twice is a no-op in Qt
    // but installing a *stale* one after a language change is not, and the
    // application owns no reference to ours after this call.
    QCoreApplication::removeTranslator(m_qtTranslator);
    if (m_currentLanguage == QLatin1String("en") || m_qtTranslator->isEmpty())
        return;
    QCoreApplication::installTranslator(m_qtTranslator);
}

int LanguageManager::translatedStringCount() const
{
    // English is not "0 strings translated": its dictionary *is* the reference,
    // so the settings page would otherwise report a 0% coverage for the language
    // every other one is measured against.
    if (m_currentLanguage == QLatin1String("en"))
        return m_fallback.size();
    return m_qtTranslator ? m_qtTranslator->entryCount() : 0;
}

int LanguageManager::referenceStringCount() const
{
    return m_fallback.size();
}

int LanguageManager::stringCountFor(const QString &code) const
{
    if (code == QLatin1String("en"))
        return m_fallback.size();   // the reference: complete by definition
    QJsonObject dictionary;
    // loadDictionary() records where it read from; that is about the *active*
    // dictionary, so it is restored rather than overwritten by a peek at another
    // language.
    const QString savedSource = m_source;
    const bool loaded = loadDictionary(code, &dictionary);
    m_source = savedSource;
    if (!loaded)
        return 0;

    int count = 0;
    for (auto it = m_fallback.constBegin(); it != m_fallback.constEnd(); ++it) {
        const QString translated = dictionary.value(it.key()).toString();
        if (!translated.isEmpty() && translated != it.value().toString())
            ++count;
    }
    return count;
}

QString LanguageManager::preference() const {
    return m_preference;
}

QString LanguageManager::sourceDescription() const {
    return m_source;
}

// The dictionary location used to be the *only* thing that mattered, and it was
// fragile: it needed an `internal_pages` folder next to the binary purely as a
// marker for "this is the app root". An installed build that ships lang/ but not
// that folder silently fell back to English forever, with no way to tell why.
//
// The dictionaries are now compiled into the binary (resources.qrc), so a build
// always has them. The external lang/ folder next to the app root is read first
// when it exists, which keeps the old workflow — drop a file in lang/ and test a
// translation without rebuilding — working exactly as before.
bool LanguageManager::loadDictionary(const QString &code, QJsonObject *out) const {
    if (!out) return false;

    const QString externalPath =
            getAppRootDir() + QStringLiteral("/lang/") + code + QStringLiteral(".json");
    QFile external(externalPath);
    if (external.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(external.readAll());
        if (doc.isObject() && !doc.object().isEmpty()) {
            *out = doc.object();
            m_source = externalPath;
            return true;
        }
    }

    // Same resources.qrc bundle that carries the internal pages, so it is there
    // in every build that has an interface at all.
    QFile bundled(QStringLiteral(":/lang/") + code + QStringLiteral(".json"));
    if (bundled.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(bundled.readAll());
        if (doc.isObject() && !doc.object().isEmpty()) {
            *out = doc.object();
            m_source = QStringLiteral("built in");
            return true;
        }
    }
    return false;
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
    // English is the fallback for every missing key, and it is not optional: a
    // dictionary that failed to load leaves getText() returning raw keys, which
    // is exactly what the "lang/" folder missing produced.
    m_fallback = QJsonObject();
    loadDictionary(QStringLiteral("en"), &m_fallback);
    const QString englishSource = m_source;

    if (m_currentLanguage == QLatin1String("en")) {
        m_translations = m_fallback;
        m_source = englishSource;
        if (m_qtTranslator)
            m_qtTranslator->setDictionaries(m_fallback, QJsonObject());
        installOnApplication();
        return;
    }

    m_translations = QJsonObject();
    if (!loadDictionary(m_currentLanguage, &m_translations)) {
        // Report the failure instead of pretending: getText() falls back to
        // English per key, and the settings page shows where the dictionary came
        // from so an empty m_source is visible rather than mysterious.
        m_translations = m_fallback;
        m_source.clear();
        if (m_qtTranslator)
            m_qtTranslator->setDictionaries(m_fallback, QJsonObject());
        installOnApplication();
        return;
    }
    if (m_qtTranslator)
        m_qtTranslator->setDictionaries(m_fallback, m_translations);
    installOnApplication();
}

// Resolve the stored preference, then the system locale. Called from the
// constructor before loadTranslations().
void LanguageManager::resolvePreference() {
    const QString stored = QSettings()
            .value(QStringLiteral("browser/language"), QStringLiteral("system"))
            .toString()
            .trimmed()
            .toLower();
    m_preference = (stored.isEmpty() || stored == QLatin1String("system")) ? QStringLiteral("system")
                                                                           : stored;
    if (m_preference == QLatin1String("system")) {
        detectSystemLanguage();
    } else if (m_supportedLanguages.contains(m_preference)) {
        m_currentLanguage = m_preference;
    } else {
        // An unknown stored code (hand-edited settings, a language that was
        // removed) must not leave the UI blank.
        m_preference = QStringLiteral("system");
        detectSystemLanguage();
    }
}

bool LanguageManager::setLanguage(const QString &code) {
    const QString lowerCode = code.toLower().trimmed();

    // "system" is a real value: it follows the OS locale, and re-resolving it
    // is part of setting it, so switching back to System picks up the current
    // locale instead of freezing whichever one it resolved to at startup.
    if (lowerCode == QLatin1String("system") || lowerCode.isEmpty()) {
        m_preference = QStringLiteral("system");
        QSettings().setValue(QStringLiteral("browser/language"), m_preference);
        detectSystemLanguage();
        loadTranslations();
        emit languageChanged(m_currentLanguage);
        return true;
    }

    if (!m_supportedLanguages.contains(lowerCode)) {
        // Do not silently pretend: an unsupported code is refused, and the
        // caller keeps showing whatever is already in force.
        return false;
    }

    const bool sameLanguage = (m_currentLanguage == lowerCode) && !m_translations.isEmpty();
    m_preference = lowerCode;
    QSettings().setValue(QStringLiteral("browser/language"), m_preference);

    if (sameLanguage) {
        return true;
    }

    m_currentLanguage = lowerCode;
    loadTranslations();
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
