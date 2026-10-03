#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "stationmanager.h"
#include "storagepaths.h"

class StationManagerTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        mOriginalWorkingDirectory = QDir::currentPath();
        mOriginalApplicationName = QCoreApplication::applicationName();
        mOriginalTestMode = QStandardPaths::isTestModeEnabled();
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName("lampbox_station_test_"
                + QUuid::createUuid().toString(QUuid::Id128));
        mStandalonePath = MediaBox::StoragePaths::commonDataDirectory();
        QVERIFY(QDir::isAbsolutePath(mStandalonePath));
        QVERIFY(mStandalonePath.contains(QCoreApplication::applicationName()));
        QVERIFY(!QDir(mStandalonePath).exists());
        mMayCleanStandalonePath = true;
    }

    void cleanup()
    {
        QVERIFY(QDir::setCurrent(mOriginalWorkingDirectory));
    }

    void cleanupTestCase()
    {
        const auto restoreApplication = qScopeGuard([this] {
            QCoreApplication::setApplicationName(mOriginalApplicationName);
            QStandardPaths::setTestModeEnabled(mOriginalTestMode);
        });
        if (!mMayCleanStandalonePath)
            return;
        QVERIFY(QDir::isAbsolutePath(mStandalonePath));
        QVERIFY(mStandalonePath.contains(QCoreApplication::applicationName()));
        QCOMPARE(mStandalonePath,
                 MediaBox::StoragePaths::commonDataDirectory());
        QVERIFY(QDir(mStandalonePath).removeRecursively());
    }

    void standaloneDataMigration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir source(directory.filePath("lampbox"));
        const QDir destination(directory.filePath("mediabox"));
        const QStringList publicFiles = {"timetable/1.xml", "media/music/song.mp3",
                                         "cron/playlist.cron", "nncronlt/nncron.tab"};
        for (const QString &path : publicFiles)
            QVERIFY(writeFile(source.absoluteFilePath(path), path.toUtf8()));
        const QStringList privateFiles = {"conf/mediaboxmanager.conf", "log/manager.log",
                                          "MediaBoxPlayer.ini", "token.txt"};
        for (const QString &path : privateFiles)
            QVERIFY(writeFile(source.absoluteFilePath(path), QByteArrayLiteral("private")));
        QVERIFY(writeFile(source.absoluteFilePath("mediabox.conf"),
                          QByteArrayLiteral("[mediastation]\nmediabox_id=42\nalternative=true\n")));
        // Windows installers may prepare these empty shared directories first.
        for (const QString &path : {"timetable", "media", "cron", "nncronlt"})
            QVERIFY(destination.mkpath(path));

        StationManager station;
        QVERIFY2(station.initializeStandaloneConfiguration(destination.path(), {source.path()}),
                 qPrintable(station.lastError()));
        QCOMPARE(station.get(), destination.path());
        QCOMPARE(station.id(), 42);
        QVERIFY(station.isAlter());
        for (const QString &path : publicFiles) {
            QCOMPARE(readFile(destination.absoluteFilePath(path)), path.toUtf8());
            QCOMPARE(readFile(source.absoluteFilePath(path)), path.toUtf8());
        }
        for (const QString &path : privateFiles)
            QVERIFY(!QFileInfo::exists(destination.absoluteFilePath(path)));
        QVERIFY(QFileInfo::exists(destination.absoluteFilePath(".station-storage-initialized")));
        QVERIFY(!QFileInfo::exists(destination.absoluteFilePath(".station-storage-migration")));

        QVERIFY(QFile::remove(destination.absoluteFilePath("timetable/1.xml")));
        QVERIFY(station.initializeStandaloneConfiguration(destination.path(), {source.path()}));
        QVERIFY(!QFileInfo::exists(destination.absoluteFilePath("timetable/1.xml")));
    }

    void existingStationAndInitializedEmptyStationAreNotReimported()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir source(directory.filePath("old"));
        const QDir existing(directory.filePath("existing"));
        const QDir empty(directory.filePath("empty"));
        StationManager station;
        QVERIFY(station.initializeStandaloneConfiguration(empty.path(), {source.path()}));
        QVERIFY(writeFile(source.absoluteFilePath("timetable/old.xml"), QByteArrayLiteral("old")));
        QVERIFY(writeFile(existing.absoluteFilePath("timetable/current.xml"), QByteArrayLiteral("current")));
        QVERIFY(station.initializeStandaloneConfiguration(existing.path(), {source.path()}));
        QCOMPARE(readFile(existing.absoluteFilePath("timetable/current.xml")), QByteArrayLiteral("current"));
        QVERIFY(!QFileInfo::exists(existing.absoluteFilePath("timetable/old.xml")));
        QVERIFY(station.initializeStandaloneConfiguration(empty.path(), {source.path()}));
        QVERIFY(!QFileInfo::exists(empty.absoluteFilePath("timetable/old.xml")));
    }

    void migrationResumesAfterFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir source(directory.filePath("old"));
        const QDir destination(directory.filePath("shared"));
        QVERIFY(writeFile(source.absoluteFilePath("timetable/1.xml"), QByteArrayLiteral("schedule")));
        // A file where nncronlt must be a directory makes initialization fail
        // after the first schedule has already been copied.
        QVERIFY(writeFile(source.absoluteFilePath("nncronlt"), QByteArrayLiteral("invalid directory")));
        StationManager station;
        QVERIFY(!station.initializeStandaloneConfiguration(destination.path(), {source.path()}));
        QVERIFY(!station.lastError().isEmpty());
        QVERIFY(QFileInfo::exists(destination.absoluteFilePath(".station-storage-migration")));
        QVERIFY(!QFileInfo::exists(destination.absoluteFilePath(".station-storage-initialized")));
        QCOMPARE(readFile(destination.absoluteFilePath("timetable/1.xml")), QByteArrayLiteral("schedule"));

        QVERIFY(QFile::remove(source.absoluteFilePath("nncronlt")));
        QVERIFY(QFile::remove(destination.absoluteFilePath("nncronlt")));
        QVERIFY(writeFile(source.absoluteFilePath("nncronlt/jobs.tab"), QByteArrayLiteral("jobs")));
        QVERIFY2(station.initializeStandaloneConfiguration(destination.path(), {}),
                 qPrintable(station.lastError()));
        QCOMPARE(readFile(destination.absoluteFilePath("nncronlt/jobs.tab")), QByteArrayLiteral("jobs"));
        QVERIFY(!QFileInfo::exists(destination.absoluteFilePath(".station-storage-migration")));
    }

    void migratedConfigurationPreservesContentPaths()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir source(directory.filePath("old"));
        const QDir destination(directory.filePath("shared"));
        QVERIFY(writeFile(source.absoluteFilePath("media/music/song.mp3"), QByteArrayLiteral("song")));
        QVERIFY(writeFile(source.absoluteFilePath("custom_cron/jobs.tab"), QByteArrayLiteral("jobs")));
        {
            QSettings settings(source.absoluteFilePath("mediabox.conf"), QSettings::IniFormat);
            settings.setValue("mediastation/media", source.absoluteFilePath("media"));
            settings.setValue("mediastation/crondir", "custom_cron");
            settings.sync();
            QCOMPARE(settings.status(), QSettings::NoError);
        }
        StationManager station;
        QVERIFY2(station.initializeStandaloneConfiguration(destination.path(), {source.path()}),
                 qPrintable(station.lastError()));
        QCOMPARE(station.media("music/song.mp3"), destination.absoluteFilePath("media/music/song.mp3"));
        QCOMPARE(station.getCronDir(), source.absoluteFilePath("custom_cron"));
        QSettings original(source.absoluteFilePath("mediabox.conf"), QSettings::IniFormat);
        QCOMPARE(original.value("mediastation/crondir").toString(), QStringLiteral("custom_cron"));
    }

    void migrationDoesNotFollowSymbolicLinks()
    {
#ifndef Q_OS_UNIX
        QSKIP("Directory symbolic-link behavior is tested on Unix.");
#else
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir source(directory.filePath("old"));
        const QDir destination(directory.filePath("shared"));
        QVERIFY(source.mkpath("media"));
        QVERIFY(QFile::link(source.absoluteFilePath("media"), source.absoluteFilePath("media/loop")));
        StationManager station;
        QVERIFY(!station.initializeStandaloneConfiguration(destination.path(), {source.path()}));
        QVERIFY(!station.lastError().isEmpty());
        QVERIFY(!QFileInfo::exists(destination.absoluteFilePath(".station-storage-initialized")));
        QVERIFY(!QFileInfo::exists(destination.absoluteFilePath("media/loop")));
#endif
    }

    void invalidMigratedConfigurationDoesNotInitializeAnEmptyStation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QDir source(directory.filePath("old"));
        const QDir destination(directory.filePath("shared"));
        const QByteArray invalidConfiguration("[invalid section\nmediabox_id=42\n");
        QVERIFY(writeFile(source.absoluteFilePath("mediabox.conf"), invalidConfiguration));
        StationManager station;
        QVERIFY(!station.initializeStandaloneConfiguration(destination.path(), {source.path()}));
        QVERIFY(!station.lastError().isEmpty());
        QVERIFY(!QFileInfo::exists(destination.absoluteFilePath(".station-storage-initialized")));
        QVERIFY(QFileInfo::exists(destination.absoluteFilePath(".station-storage-migration")));
        QCOMPARE(readFile(source.absoluteFilePath("mediabox.conf")), invalidConfiguration);
    }

    void standalonePathsRemainStable()
    {
        StationManager station;
        QVERIFY(station.initializeStandaloneConfiguration());
        const QString home = station.get();
        const QString media = station.media("");
        const QString config = station.configFile();
        const QString cron = station.getCronDir();
        QCOMPARE(home, QDir::cleanPath(mStandalonePath));
        QVERIFY(QDir::isAbsolutePath(home));
        QVERIFY(QDir::isAbsolutePath(media));
        QVERIFY(QDir::isAbsolutePath(config));
        QVERIFY(QDir::isAbsolutePath(cron));

        const QStringList directories = {"timetable", "media/music", "media/video",
                                         "media/ads", "nncronlt", "cron"};
        for (const QString &directory : directories)
            QVERIFY2(QDir(station.get(directory)).exists(), qPrintable(directory));

        QTemporaryDir workingDirectory;
        QVERIFY(workingDirectory.isValid());
        const auto restoreWorkingDirectory = qScopeGuard([this] {
            QDir::setCurrent(mOriginalWorkingDirectory);
        });
        QVERIFY(QDir::setCurrent(workingDirectory.path()));
        QVERIFY(station.initializeStandaloneConfiguration());
        QCOMPARE(station.get(), home);
        QCOMPARE(station.media(""), media);
        QCOMPARE(station.configFile(), config);
        QCOMPARE(station.getCronDir(), cron);
        QCOMPARE(station.type(), STATION_LOCAL);
        QCOMPARE(station.id(), LOCAL_ID);
        QVERIFY(!station.trial());
        QVERIFY(!station.isAlter());
    }

    void configuredStationUsesItsOwnPaths()
    {
        QTemporaryDir directory;
        QTemporaryDir workingDirectory;
        QVERIFY(directory.isValid());
        QVERIFY(workingDirectory.isValid());
        const auto restoreWorkingDirectory = qScopeGuard([this] {
            QDir::setCurrent(mOriginalWorkingDirectory);
        });
        {
            QSettings settings(directory.filePath("mediabox.conf"), QSettings::IniFormat);
            settings.setValue("mediastation/media", "content");
            settings.setValue("mediastation/crondir", "cron_jobs");
            settings.setValue("mediastation/mediabox_id", 42);
            settings.setValue("mediastation/alternative", true);
            settings.sync();
            QCOMPARE(settings.status(), QSettings::NoError);
        }

        StationManager station;
        QVERIFY(station.loadConfiguration(directory.path(), STATION_NETWORK, true));
        QCOMPARE(station.get(), directory.path());
        QCOMPARE(station.media("music"), directory.filePath("content/music"));
        QCOMPARE(station.getCronDir(), directory.filePath("cron_jobs"));
        QCOMPARE(station.configFile(), directory.filePath("mediabox.conf"));
        QCOMPARE(station.type(), STATION_NETWORK);
        QCOMPARE(station.id(), 42);
        QVERIFY(station.trial());
        QVERIFY(station.isAlter());
        QVERIFY(!QDir(directory.filePath("content")).exists());
        QVERIFY(!QDir(directory.filePath("timetable")).exists());

        QVERIFY(QDir::setCurrent(workingDirectory.path()));
        QVERIFY(station.loadConfiguration(directory.path(), STATION_NETWORK, true));
        QCOMPARE(station.media("music"), directory.filePath("content/music"));
        QCOMPARE(station.getCronDir(), directory.filePath("cron_jobs"));

        QVERIFY(station.initializeStandaloneConfiguration());
        QCOMPARE(station.id(), LOCAL_ID);
        QCOMPARE(station.type(), STATION_LOCAL);
        QVERIFY(!station.trial());
        QVERIFY(!station.isAlter());
    }

    void absentMediaUsesStationDirectory()
    {
        QTemporaryDir directory;
        QTemporaryDir workingDirectory;
        QVERIFY(directory.isValid());
        QVERIFY(workingDirectory.isValid());
        const auto restoreWorkingDirectory = qScopeGuard([this] {
            QDir::setCurrent(mOriginalWorkingDirectory);
        });
        {
            QSettings settings(directory.filePath("mediabox.conf"), QSettings::IniFormat);
            settings.setValue("mediastation/mediabox_id", 7);
            settings.sync();
            QCOMPARE(settings.status(), QSettings::NoError);
        }
        QVERIFY(QDir::setCurrent(workingDirectory.path()));

        StationManager station;
        QVERIFY(station.loadConfiguration(directory.path(), STATION_LOCAL, false));
        QCOMPARE(station.media("music"), directory.filePath("media/music"));
        QCOMPARE(station.getCronDir(), directory.filePath("cron"));
        QVERIFY(station.media("") != QDir::currentPath());
        QVERIFY(!station.loadConfiguration("", STATION_LOCAL, false));
        QVERIFY(!station.loadConfiguration("relative/path", STATION_LOCAL, false));
        QVERIFY(!station.loadConfiguration(workingDirectory.path(), STATION_LOCAL, false));
        QCOMPARE(station.get(), directory.path());
    }

private:
    static bool writeFile(const QString &path, const QByteArray &contents)
    {
        if (!QDir().mkpath(QFileInfo(path).absolutePath()))
            return false;
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
    }

    static QByteArray readFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    }

    QString mOriginalWorkingDirectory;
    QString mOriginalApplicationName;
    QString mStandalonePath;
    bool mOriginalTestMode = false;
    bool mMayCleanStandalonePath = false;
};

QTEST_GUILESS_MAIN(StationManagerTest)
#include "tst_stationmanager.moc"
