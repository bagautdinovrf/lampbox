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
        QCoreApplication::setApplicationName("mediaboxmanager_station_test_"
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

    void standaloneInitializesOnlyRequestedDirectory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString stationPath = directory.filePath("station");
        StationManager station;
        QVERIFY2(station.initializeStandaloneConfiguration(stationPath), qPrintable(station.lastError()));
        QCOMPARE(station.get(), stationPath);
        for (const QString &kind : {"music", "video", "ads"})
            QVERIFY(QDir(station.media(kind)).exists());
        const QByteArray project("current project contents");
        QVERIFY(writeFile(station.get("project.json"), project));
        QVERIFY(station.initializeStandaloneConfiguration(stationPath));
        QCOMPARE(readFile(station.get("project.json")), project);
        const QString blocked = directory.filePath("blocked");
        QVERIFY(writeFile(blocked, "not a directory"));
        QVERIFY(!station.initializeStandaloneConfiguration(blocked));
        QVERIFY(!station.lastError().isEmpty());
    }

    void standalonePathsRemainStable()
    {
        StationManager station;
        QVERIFY(station.initializeStandaloneConfiguration());
        const QString home = station.get();
        const QString media = station.media("");
        const QString config = station.configFile();
        QCOMPARE(home, QDir::cleanPath(mStandalonePath));
        QVERIFY(QDir::isAbsolutePath(home));
        QVERIFY(QDir::isAbsolutePath(media));
        QVERIFY(QDir::isAbsolutePath(config));

        const QStringList directories = {"media/music", "media/video", "media/ads"};
        for (const QString &directory : directories)
            QVERIFY2(QDir(station.get(directory)).exists(), qPrintable(directory));
        for (const QString &directory : {"timetable", "nncronlt", "cron"})
            QVERIFY2(!QDir(station.get(directory)).exists(), qPrintable(directory));

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
        QCOMPARE(station.type(), STATION_LOCAL);
        QCOMPARE(station.id(), LOCAL_ID);
        QVERIFY(!station.trial());
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
        QCOMPARE(station.configFile(), directory.filePath("mediabox.conf"));
        QCOMPARE(station.type(), STATION_NETWORK);
        QCOMPARE(station.id(), 42);
        QVERIFY(station.trial());
        QVERIFY(!QDir(directory.filePath("content")).exists());
        QVERIFY(!QDir(directory.filePath("timetable")).exists());

        QVERIFY(QDir::setCurrent(workingDirectory.path()));
        QVERIFY(station.loadConfiguration(directory.path(), STATION_NETWORK, true));
        QCOMPARE(station.media("music"), directory.filePath("content/music"));

        QVERIFY(station.initializeStandaloneConfiguration());
        QCOMPARE(station.id(), LOCAL_ID);
        QCOMPARE(station.type(), STATION_LOCAL);
        QVERIFY(!station.trial());
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
