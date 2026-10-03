#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "stationmanager.h"

class StationManagerTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        mOriginalWorkingDirectory = QDir::currentPath();
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName("lampbox_station_test_"
                + QUuid::createUuid().toString(QUuid::Id128));
        mStandalonePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
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
        if (!mMayCleanStandalonePath)
            return;
        QVERIFY(QDir::isAbsolutePath(mStandalonePath));
        QVERIFY(mStandalonePath.contains(QCoreApplication::applicationName()));
        QCOMPARE(mStandalonePath,
                 QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        QVERIFY(QDir(mStandalonePath).removeRecursively());
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
    QString mOriginalWorkingDirectory;
    QString mStandalonePath;
    bool mMayCleanStandalonePath = false;
};

QTEST_GUILESS_MAIN(StationManagerTest)
#include "tst_stationmanager.moc"
