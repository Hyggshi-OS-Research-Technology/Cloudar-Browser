#include "LocalFileService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QTemporaryDir>
#include <QtTest>

// ---------------------------------------------------------------------------
// The one thing in this feature that is a *security* boundary: which paths a
// cloudar:// page can make the browser enumerate.
//
// LocalFileService::listDirectory() is reachable from page JavaScript through
// the `internalBridge` QWebChannel object, and a cloudar:// page is treated by
// Chromium as local content. Every test below is a path that must be refused
// unless the user opened it. See docs/FILE_PROTOCOL_SECURITY.md.
// ---------------------------------------------------------------------------
class TestLocalFileService : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void resolveIgnoresBareWords();
    void resolveFindsAbsolutePath();
    void resolveExpandsTilde();
    void resolveRejectsMissingPath();
    void resolveHonoursExplicitFileUrl();
    void resolveStripsPastedQuotes();

    void listingRefusedWithoutRoot();
    void listingRefusedOutsideRoot();
    void listingAllowedInsideRoot();
    void listingRefusedForSiblingPrefix();
    void listingNeverEscapesViaParent();
    void listingNeverEscapesViaDotDot();
    void listingRefusedForFile();
    void listingRefusedViaSymlink();
    void listingReportsEntries();

private:
    QTemporaryDir *m_tmp = nullptr;
    QString m_root;   // opened by the user
    QString m_outside; // never opened
    LocalFileService *m_service = nullptr;
};

void TestLocalFileService::init()
{
    m_tmp = new QTemporaryDir;
    QVERIFY(m_tmp->isValid());
    // Canonicalised up front: on a machine where /tmp (or $TMPDIR) is itself a
    // symlink, every expected path below would otherwise differ from what the
    // service returns for reasons that have nothing to do with the logic.
    const QString base = QFileInfo(m_tmp->path()).canonicalFilePath();
    QVERIFY(!base.isEmpty());

    // Two sibling trees. "user" is the root the user opens; "user2" exists so
    // that a naive startsWith() check (which would consider /tmp/x/user2 to be
    // inside /tmp/x/user) fails the test instead of passing it.
    m_root = base + "/user";
    m_outside = base + "/user2";
    QDir().mkpath(m_root + "/sub/deeper");
    QDir().mkpath(m_outside);

    auto write = [](const QString &path, const QByteArray &contents) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return false;
        return f.write(contents) == contents.size();
    };
    QVERIFY(write(m_root + "/notes.txt", "hello"));
    QVERIFY(write(m_root + "/sub/inner.txt", "inner"));
    QVERIFY(write(m_outside + "/secret.txt", "secret"));

    m_service = new LocalFileService;
}

void TestLocalFileService::cleanup()
{
    delete m_service;
    m_service = nullptr;
    delete m_tmp;
    m_tmp = nullptr;
}

// --- address bar -----------------------------------------------------------

void TestLocalFileService::resolveIgnoresBareWords()
{
    // A bare word is a search query even when a directory of that name happens
    // to exist next to the working directory: a launched GUI app's cwd is not
    // something the user can see, so honouring it would be a surprise.
    QString path;
    QVERIFY(LocalFileService::resolveUserInput(QStringLiteral("user"), &path).isEmpty());
    QVERIFY(path.isEmpty());
    QVERIFY(LocalFileService::resolveUserInput(QStringLiteral("example.com")).isEmpty());
    QVERIFY(LocalFileService::resolveUserInput(QStringLiteral("hello world")).isEmpty());
    QVERIFY(LocalFileService::resolveUserInput(QString()).isEmpty());
}

void TestLocalFileService::resolveFindsAbsolutePath()
{
    QString path;
    const QUrl url = LocalFileService::resolveUserInput(m_root + "/notes.txt", &path);
    QVERIFY(url.isValid());
    QCOMPARE(url.scheme(), QStringLiteral("file"));
    QCOMPARE(path, m_root + "/notes.txt");

    // A directory has no dot in it, which is exactly the case the old
    // "does the text contain a dot?" heuristic sent to a search engine.
    QVERIFY(!LocalFileService::resolveUserInput(m_root, &path).isEmpty());
    QCOMPARE(path, m_root);
}

void TestLocalFileService::resolveExpandsTilde()
{
    QString path;
    const QUrl url = LocalFileService::resolveUserInput(QStringLiteral("~"), &path);
    QVERIFY(!url.isEmpty());
    QCOMPARE(path, QDir::homePath());

    // Only a leading "~/" expands; "a~b" is a legal filename and must not be
    // rewritten.
    QVERIFY(LocalFileService::resolveUserInput(QStringLiteral("a~b/c"), &path).isEmpty());
}

void TestLocalFileService::resolveRejectsMissingPath()
{
    QString path;
    // Absolute, well formed, and does not exist: still not something to open.
    // (A file:// URL is the exception, handled below, so that Chromium shows
    // its own "not found" page rather than a search results page.)
    QVERIFY(LocalFileService::resolveUserInput(m_root + "/nope.txt", &path).isEmpty());
    QVERIFY(path.isEmpty());
}

void TestLocalFileService::resolveHonoursExplicitFileUrl()
{
    QString path;
    const QUrl url = LocalFileService::resolveUserInput(QUrl::fromLocalFile(m_root + "/notes.txt").toString(),
                                                       &path);
    QVERIFY(!url.isEmpty());
    QCOMPARE(url.scheme(), QStringLiteral("file"));
    QCOMPARE(path, m_root + "/notes.txt");

    // Even a file:// URL that points nowhere keeps its scheme: the page that
    // comes up is Chromium's error, which tells the user what happened.
    QVERIFY(!LocalFileService::resolveUserInput(QStringLiteral("file:///definitely/not/here")).isEmpty());
}

void TestLocalFileService::resolveStripsPastedQuotes()
{
    QString path;
    const QUrl url = LocalFileService::resolveUserInput(QStringLiteral("\"") + m_root + "\"", &path);
    QVERIFY(!url.isEmpty());
    QCOMPARE(path, m_root);
}

// --- confinement -----------------------------------------------------------

void TestLocalFileService::listingRefusedWithoutRoot()
{
    const QVariantMap result = m_service->listDirectory(QString());
    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(result.value(QStringLiteral("error")).toString(), QStringLiteral("no-root"));

    // An explicit path is refused too while nothing has been opened.
    const QVariantMap explicitPath = m_service->listDirectory(m_root);
    QCOMPARE(explicitPath.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(explicitPath.value(QStringLiteral("error")).toString(), QStringLiteral("outside-root"));
}

void TestLocalFileService::listingRefusedOutsideRoot()
{
    m_service->addRoot(m_root);
    const QVariantMap result = m_service->listDirectory(m_outside);
    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(result.value(QStringLiteral("error")).toString(), QStringLiteral("outside-root"));
}

void TestLocalFileService::listingAllowedInsideRoot()
{
    m_service->addRoot(m_root);

    const QVariantMap rootListing = m_service->listDirectory(QString());
    QCOMPARE(rootListing.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(rootListing.value(QStringLiteral("path")).toString(), m_root);

    // A subdirectory is inside the root, so it is listable.
    const QVariantMap sub = m_service->listDirectory(m_root + "/sub");
    QCOMPARE(sub.value(QStringLiteral("ok")).toBool(), true);
}

void TestLocalFileService::listingRefusedForSiblingPrefix()
{
    m_service->addRoot(m_root);
    // The classic string-prefix bug: "/tmp/x/user2" starts with "/tmp/x/user".
    // Component-wise comparison is what makes this a refusal.
    QVERIFY(!m_service->isUnderRoot(m_outside));
    QVERIFY(m_service->isUnderRoot(m_root));
    QVERIFY(m_service->isUnderRoot(m_root + "/sub"));
    QCOMPARE(m_service->listDirectory(m_outside).value(QStringLiteral("error")).toString(),
             QStringLiteral("outside-root"));
}

void TestLocalFileService::listingNeverEscapesViaParent()
{
    m_service->addRoot(m_root);

    // Walking up from the root must stop at the root: the listing may not offer
    // a ".." that leaves the confinement.
    const QVariantMap atRoot = m_service->listDirectory(m_root);
    QCOMPARE(atRoot.value(QStringLiteral("ok")).toBool(), true);
    QVERIFY(atRoot.value(QStringLiteral("parent")).toString().isEmpty());

    // One level down, the parent exists and is the root itself.
    const QVariantMap sub = m_service->listDirectory(m_root + "/sub");
    QCOMPARE(sub.value(QStringLiteral("parent")).toString(), m_root);
}

void TestLocalFileService::listingNeverEscapesViaDotDot()
{
    m_service->addRoot(m_root);
    // Canonicalisation resolves ".." before the check, so neither of these is a
    // way out. The first is still inside; the second is not.
    QCOMPARE(m_service->listDirectory(m_root + "/sub/..").value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(m_service->listDirectory(m_root + "/../user2").value(QStringLiteral("error")).toString(),
             QStringLiteral("outside-root"));
}

void TestLocalFileService::listingRefusedViaSymlink()
{
    m_service->addRoot(m_root);

    // A symlink *inside* the root that points outside it. canonicalFilePath()
    // resolves the link before the comparison, so the path the page asks for
    // resolves to /tmp/.../user2 and is refused. Without canonicalisation this
    // would be the cleanest way out of the confinement, and a symlink is
    // something a user can create by accident just by dragging a folder.
    const QString link = m_root + "/not-really-inside";
    if (!QFile::link(m_outside, link))
        QSKIP("cannot create symlinks here (Windows without developer mode)");

    QCOMPARE(m_service->listDirectory(link).value(QStringLiteral("error")).toString(),
             QStringLiteral("outside-root"));

    // The same escape attempted one level up, through a *directory* symlink, so
    // that even the "parent" of a legitimate listing cannot be a way out.
    QCOMPARE(m_service->listDirectory(link + "/..").value(QStringLiteral("ok")).toBool(), false);
}

void TestLocalFileService::listingRefusedForFile()
{
    m_service->addRoot(m_root);
    // A file is inside the root, but it is not a directory: listing it would
    // mean handing the page something other than names/sizes/dates.
    const QVariantMap result = m_service->listDirectory(m_root + "/notes.txt");
    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(result.value(QStringLiteral("error")).toString(), QStringLiteral("not-a-directory"));
}

void TestLocalFileService::listingReportsEntries()
{
    m_service->addRoot(m_root);
    const QVariantMap result = m_service->listDirectory(QString());
    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), true);

    const QVariantList entries = result.value(QStringLiteral("entries")).toList();
    QCOMPARE(entries.size(), 2); // notes.txt and sub

    bool sawFile = false;
    bool sawDirectory = false;
    for (const QVariant &value : entries) {
        const QVariantMap entry = value.toMap();
        // Every entry carries the URL the page navigates to, built by QUrl
        // rather than by hand in JavaScript.
        const QUrl url(entry.value(QStringLiteral("url")).toString());
        QCOMPARE(url.scheme(), QStringLiteral("file"));
        QVERIFY(QFileInfo::exists(url.toLocalFile()));

        if (entry.value(QStringLiteral("name")).toString() == QLatin1String("notes.txt")) {
            sawFile = true;
            QCOMPARE(entry.value(QStringLiteral("isDir")).toBool(), false);
            QCOMPARE(entry.value(QStringLiteral("size")).toLongLong(), 5);
        } else if (entry.value(QStringLiteral("name")).toString() == QLatin1String("sub")) {
            sawDirectory = true;
            QCOMPARE(entry.value(QStringLiteral("isDir")).toBool(), true);
        }
    }
    QVERIFY(sawFile);
    QVERIFY(sawDirectory);

    // Directories sort before files, which is what the page relies on.
    QVERIFY(entries.first().toMap().value(QStringLiteral("isDir")).toBool());
}

// GUILESS, not MAIN: nothing here needs a QApplication or a display, and on a
// headless CI runner a widget test would fail before it ran a single check.
QTEST_GUILESS_MAIN(TestLocalFileService)
#include "tst_localfileservice.moc"
