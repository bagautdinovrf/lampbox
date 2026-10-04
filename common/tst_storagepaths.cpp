#include "storagepaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

using namespace MediaBox::StoragePaths;

namespace {
bool writeFile(const QString &path, const QByteArray &contents)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
} // namespace

class StoragePathsTest final : public QObject
{
    Q_OBJECT
private slots:
    void platformRoots()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto oldConfig = qgetenv("XDG_CONFIG_HOME");
        const auto oldData = qgetenv("XDG_DATA_HOME");
        const auto oldProgramData = qgetenv("ProgramData");
        const auto oldName = QCoreApplication::applicationName();
        const bool oldTestMode = QStandardPaths::isTestModeEnabled();
        const auto restore = qScopeGuard([&] {
            for (const auto &entry : {qMakePair("XDG_CONFIG_HOME", oldConfig),
                                      qMakePair("XDG_DATA_HOME", oldData),
                                      qMakePair("ProgramData", oldProgramData)}) {
                if (entry.second.isNull())
                    qunsetenv(entry.first);
                else
                    qputenv(entry.first, entry.second);
            }
            QCoreApplication::setApplicationName(oldName);
            QStandardPaths::setTestModeEnabled(oldTestMode);
        });
        QStandardPaths::setTestModeEnabled(false);
        qputenv("XDG_CONFIG_HOME", directory.filePath("config").toUtf8());
        qputenv("XDG_DATA_HOME", directory.filePath("data").toUtf8());
        qputenv("ProgramData", directory.filePath("ProgramData").toUtf8());
#if defined(Q_OS_WIN)
        const QString configRoot = directory.filePath("ProgramData/MediaBox");
        const QString dataRoot = configRoot;
        const QString applicationDataRoot = dataRoot;
        const QString managerName = QStringLiteral("MediaBoxManager");
        const QString playerName = QStringLiteral("MediaBoxPlayer");
        const QString videoPlayerName = QStringLiteral("MediaBoxVPlayer");
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        const QString configRoot = QStringLiteral("/etc/mediabox");
        const QString dataRoot = configRoot;
        const QString applicationDataRoot = dataRoot;
        const QString managerName = QStringLiteral("mediaboxmanager");
        const QString playerName = QStringLiteral("mediaboxplayer");
        const QString videoPlayerName = QStringLiteral("mediaboxvplayer");
#else
        QSKIP("Native desktop path contract is tested on Windows and Linux.");
#endif
#if defined(Q_OS_WIN) || (defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID))
        // Roots must not depend on which application is asking for them or on
        // XDG overrides: Linux installations use machine-wide directories.
        const bool configExisted = QFileInfo::exists(configRoot);
        const bool dataExisted = QFileInfo::exists(dataRoot);
        for (const auto &name : {"MediaBoxManager", "MediaBoxPlayer", "MediaBoxVPlayer"}) {
            QCoreApplication::setApplicationName(name);
            QCOMPARE(commonConfigurationDirectory(), configRoot);
            QCOMPARE(commonDataDirectory(), dataRoot);
            QCOMPARE(configurationDirectory(Application::Manager), configRoot + '/' + managerName);
            QCOMPARE(configurationDirectory(Application::Player), configRoot + '/' + playerName);
            QCOMPARE(configurationDirectory(Application::VideoPlayer), configRoot + '/' + videoPlayerName);
            QCOMPARE(dataDirectory(Application::Manager), applicationDataRoot + '/' + managerName);
            QCOMPARE(dataDirectory(Application::Player), applicationDataRoot + '/' + playerName);
            QCOMPARE(dataDirectory(Application::VideoPlayer), applicationDataRoot + '/' + videoPlayerName);
        }
        QCOMPARE(QFileInfo::exists(configRoot), configExisted);
        QCOMPARE(QFileInfo::exists(dataRoot), dataExisted);
#endif
    }

    void testModeIsolatesSharedPaths()
    {
        const bool oldMode = QStandardPaths::isTestModeEnabled();
        const QString oldOrganization = QCoreApplication::organizationName();
        const auto restore = qScopeGuard([&] {
            QCoreApplication::setOrganizationName(oldOrganization);
            QStandardPaths::setTestModeEnabled(oldMode);
        });
        const QString organization = "StoragePaths_" + QUuid::createUuid().toString(QUuid::Id128);
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(organization);
        QVERIFY(commonConfigurationDirectory().contains(organization));
        QVERIFY(commonDataDirectory().contains(organization));
        for (const auto application : {Application::Manager, Application::Player, Application::VideoPlayer}) {
            QVERIFY(configurationDirectory(application).contains(organization));
            QVERIFY(dataDirectory(application).contains(organization));
        }
        QVERIFY(!QFileInfo::exists(commonConfigurationDirectory()));
        QVERIFY(!QFileInfo::exists(commonDataDirectory()));
    }

    void migrationPreservesSourcesAndNewSettings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = directory.filePath("old/config");
        const QString second = directory.filePath("older/config");
        const QString target = directory.filePath("new/app/config");
        QVERIFY(writeFile(first, "first"));
        QVERIFY(writeFile(second, "second"));
        QString error;
        QVERIFY2(migrateFile(target, {directory.filePath("absent"), first, second}, &error), qPrintable(error));
        QCOMPARE(readFile(target), QByteArray("first"));
        QCOMPARE(readFile(first), QByteArray("first"));
        QCOMPARE(readFile(second), QByteArray("second"));
        QVERIFY(writeFile(target, "new"));
        QVERIFY(migrateFile(target, {first, second}, &error));
        QCOMPARE(readFile(target), QByteArray("new"));
        QVERIFY(error.isEmpty());
    }

    void invalidDestinationDoesNotModifySource()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString source = directory.filePath("source");
        const QString target = directory.filePath("target");
        QVERIFY(writeFile(source, "original"));
        QVERIFY(QDir().mkpath(target));
        QString error;
        QVERIFY(!migrateFile(target, {source}, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(readFile(source), QByteArray("original"));
        QVERIFY(!migrateFile(source + "/blocked", {source}, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(readFile(source), QByteArray("original"));
    }

    void migrationKeepsPrivateFilePermissions()
    {
#ifdef Q_OS_UNIX
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString source = directory.filePath("control.token");
        const QString target = directory.filePath("player/control.token");
        QVERIFY(writeFile(source, "private token"));
        QVERIFY(QFile::setPermissions(source, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        QVERIFY(migrateFile(target, {source}));
        QVERIFY(!(QFile::permissions(target) & (QFileDevice::ReadGroup | QFileDevice::WriteGroup
                                               | QFileDevice::ReadOther | QFileDevice::WriteOther)));
#endif
    }
};

QTEST_GUILESS_MAIN(StoragePathsTest)
#include "tst_storagepaths.moc"
