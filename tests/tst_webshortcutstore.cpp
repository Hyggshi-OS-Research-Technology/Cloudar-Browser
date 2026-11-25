#include "WebShortcutStore.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

// ---------------------------------------------------------------------------
// Web shortcut storage (roadmap item 7).
//
// Not a security boundary like LocalFileService, but it has three invariants
// that a menu depends on and that are easy to break silently:
//
//   1. one site, one shortcut — adding the same URL twice must update the name,
//      not append a duplicate the user cannot tell apart in the menu;
//   2. the id survives a rename, because a launcher pinned to it would otherwise
//      stop working;
//   3. file:// and cloudar:// are refused. A chrome-less window on file:/// is a
//      second route to local files that bypasses the confinement documented in
//      docs/FILE_PROTOCOL_SECURITY.md, so the refusal is the point.
// ---------------------------------------------------------------------------
class TestWebShortcutStore : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void refusesNonWebUrls();
    void addsAndFinds();
    void updateInPlaceKeepsId();
    void trailingSlashAndFragmentAreTheSameSite();
    void emptyNameFallsBackToHost();
    void removeById();
    void survivesCorruptStorage();

private:
    QTemporaryDir *m_dir = nullptr;
};

void TestWebShortcutStore::init()
{
    m_dir = new QTemporaryDir;
    QVERIFY(m_dir->isValid());
    // A per-test settings file, so the developer's real shortcuts are untouched
    // and one test cannot leak state into the next.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir->path());
    QCoreApplication::setOrganizationName(QStringLiteral("CloudarTest"));
    QCoreApplication::setApplicationName(QStringLiteral("WebShortcutStoreTest"));
    QSettings().clear();
}

void TestWebShortcutStore::cleanup()
{
    QSettings().clear();
    delete m_dir;
    m_dir = nullptr;
}

void TestWebShortcutStore::refusesNonWebUrls()
{
    QVERIFY(!WebShortcutStore::isShortcutableUrl(QUrl(QStringLiteral("file:///etc/passwd"))));
    QVERIFY(!WebShortcutStore::isShortcutableUrl(QUrl(QStringLiteral("cloudar://settings"))));
    QVERIFY(!WebShortcutStore::isShortcutableUrl(QUrl(QStringLiteral("javascript:alert(1)"))));
    QVERIFY(!WebShortcutStore::isShortcutableUrl(QUrl()));
    QVERIFY(!WebShortcutStore::isShortcutableUrl(QUrl(QStringLiteral("https://"))));
    QVERIFY(WebShortcutStore::isShortcutableUrl(QUrl(QStringLiteral("https://example.com"))));
    QVERIFY(WebShortcutStore::isShortcutableUrl(QUrl(QStringLiteral("http://localhost:8080/x"))));

    WebShortcutStore store;
    QVERIFY(!store.add(QStringLiteral("Passwd"), QUrl(QStringLiteral("file:///etc/passwd"))).isValid());
    QCOMPARE(store.all().size(), 0);
}

void TestWebShortcutStore::addsAndFinds()
{
    WebShortcutStore store;
    const auto created = store.add(QStringLiteral("Example"), QUrl(QStringLiteral("https://example.com")));
    QVERIFY(created.isValid());
    QVERIFY(!created.id.isEmpty());
    QCOMPARE(created.name, QStringLiteral("Example"));
    QVERIFY(!created.added.isEmpty());

    QCOMPARE(store.all().size(), 1);
    QCOMPARE(store.find(created.id).name, QStringLiteral("Example"));
    QCOMPARE(store.findByUrl(QUrl(QStringLiteral("https://example.com"))).id, created.id);

    // A fresh store over the same QSettings is what a relaunch looks like.
    WebShortcutStore reopened;
    QCOMPARE(reopened.all().size(), 1);
}

void TestWebShortcutStore::updateInPlaceKeepsId()
{
    WebShortcutStore store;
    const auto first = store.add(QStringLiteral("Old name"), QUrl(QStringLiteral("https://example.com")));
    const auto second = store.add(QStringLiteral("New name"), QUrl(QStringLiteral("https://example.com")));

    QCOMPARE(store.all().size(), 1);
    QCOMPARE(second.id, first.id);          // a pinned launcher still resolves
    QCOMPARE(second.name, QStringLiteral("New name"));
}

void TestWebShortcutStore::trailingSlashAndFragmentAreTheSameSite()
{
    WebShortcutStore store;
    store.add(QStringLiteral("Site"), QUrl(QStringLiteral("https://example.com/app")));
    store.add(QStringLiteral("Site again"), QUrl(QStringLiteral("https://example.com/app/")));
    store.add(QStringLiteral("Site fragment"), QUrl(QStringLiteral("https://example.com/app#top")));
    QCOMPARE(store.all().size(), 1);

    // A different *path* is a different shortcut: a shortcut to /app and one to
    // /admin are two different windows, which is the useful behaviour.
    store.add(QStringLiteral("Admin"), QUrl(QStringLiteral("https://example.com/admin")));
    QCOMPARE(store.all().size(), 2);
}

void TestWebShortcutStore::emptyNameFallsBackToHost()
{
    WebShortcutStore store;
    const auto created = store.add(QStringLiteral("   "), QUrl(QStringLiteral("https://docs.example.com/x")));
    QVERIFY(created.isValid());
    QCOMPARE(created.name, QStringLiteral("docs.example.com"));
}

void TestWebShortcutStore::removeById()
{
    WebShortcutStore store;
    const auto keep = store.add(QStringLiteral("Keep"), QUrl(QStringLiteral("https://keep.example")));
    const auto drop = store.add(QStringLiteral("Drop"), QUrl(QStringLiteral("https://drop.example")));
    QCOMPARE(store.all().size(), 2);

    QVERIFY(store.remove(drop.id));
    QCOMPARE(store.all().size(), 1);
    QCOMPARE(store.all().first().id, keep.id);

    // Removing something that is not there is a no-op, not an error the caller
    // has to defend against.
    QVERIFY(!store.remove(drop.id));
    QVERIFY(!store.remove(QString()));
}

void TestWebShortcutStore::survivesCorruptStorage()
{
    // A hand-edited or truncated value must read as "no shortcuts", not crash.
    QSettings().setValue(QStringLiteral("browser/webShortcuts"), QStringLiteral("{not json"));
    WebShortcutStore store;
    QCOMPARE(store.all().size(), 0);

    // And it recovers: adding writes a fresh, valid array.
    QVERIFY(store.add(QStringLiteral("Ok"), QUrl(QStringLiteral("https://ok.example"))).isValid());
    QCOMPARE(store.all().size(), 1);
}

QTEST_GUILESS_MAIN(TestWebShortcutStore)
#include "tst_webshortcutstore.moc"
