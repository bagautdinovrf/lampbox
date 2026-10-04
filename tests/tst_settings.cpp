#include <utility>
#include <stdexcept>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSettings>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "boxlog.h"
#include "settings.h"
#include "storagepaths.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {
class SettingsEnvironment final
{
public:
    SettingsEnvironment()
        : originalName(QCoreApplication::applicationName()),
          originalOrganization(QCoreApplication::organizationName()),
          originalTestMode(QStandardPaths::isTestModeEnabled()),
          originalPreviewFile(QCoreApplication::instance()->property("restylePreviewSettings")),
          testOrganization("MediaBoxManagerSettingsTest_" + QUuid::createUuid().toString(QUuid::Id128))
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName("MediaBoxManager");
        QCoreApplication::setOrganizationName(testOrganization);
        QCoreApplication::instance()->setProperty("restylePreviewSettings", QVariant());
        ownedDirectories << QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                         << QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        ownedDirectories.removeDuplicates();
    }

    ~SettingsEnvironment()
    {
        for (const QString &directory : std::as_const(ownedDirectories)) {
            if (!directory.contains(testOrganization))
                continue;
            QDir(directory).removeRecursively();
            QDir().rmdir(QFileInfo(directory).dir().absolutePath());
        }
        QCoreApplication::instance()->setProperty("restylePreviewSettings", originalPreviewFile);
        QCoreApplication::setApplicationName(originalName);
        QCoreApplication::setOrganizationName(originalOrganization);
        QStandardPaths::setTestModeEnabled(originalTestMode);
    }

private:
    QString originalName;
    QString originalOrganization;
    bool originalTestMode;
    QVariant originalPreviewFile;
    QString testOrganization;
    QStringList ownedDirectories;
};

bool writeContents(const QString &path, const QByteArray &contents)
{
    if (!QFileInfo(path).dir().mkpath("."))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

QString managerConfigurationFile()
{
    return QDir(MediaBox::StoragePaths::configurationDirectory(MediaBox::StoragePaths::Application::Manager))
            .absoluteFilePath("MediaBoxManager.conf");
}
}

class SettingsTest final : public QObject
{
    Q_OBJECT

private slots:
    void mainWindowGeometryPreservesBinaryDataAndOtherSettings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString file = directory.filePath("window.conf");
        Settings settings(file, nullptr);
        QVERIFY(settings.mainWindowGeometry().isEmpty());
        settings.setThemeId("dark");
        settings.writeStringSettings("Station/Name", "Студия");

        QByteArray geometry;
        for (int value = 0; value <= 255; ++value)
            geometry.append(static_cast<char>(value));
        QVERIFY(settings.setMainWindowGeometry(geometry));

        Settings reopened(file, nullptr);
        QCOMPARE(reopened.mainWindowGeometry(), geometry);
        QCOMPARE(reopened.themeId(), QString("dark"));
        {
            const QSettings raw(file, QSettings::IniFormat);
            QCOMPARE(raw.value("Station/Name").toString(), QString("Студия"));
        }

        QVERIFY(reopened.setMainWindowGeometry({}));
        QCOMPARE(settings.mainWindowGeometry(), QByteArray());
        const QSettings raw(file, QSettings::IniFormat);
        QVERIFY(!raw.contains("MainWindow/Geometry"));
        QCOMPARE(raw.value("Appearance/theme").toString(), QString("dark"));
        QCOMPARE(raw.value("Station/Name").toString(), QString("Студия"));
    }

    void mainWindowGeometryWriteFailureIsReported()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString file = directory.filePath("window.conf");
        Settings settings(file, nullptr);
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkpath(file));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Не удалось сохранить настройки в .*"));
        QVERIFY(!settings.setMainWindowGeometry(QByteArray("geometry")));
        QVERIFY(QFileInfo(file).isDir());
    }

    void playerConnectionDefaults()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Settings settings(directory.filePath("player.conf"), nullptr);
        const PlayerConnectionSettings connection = settings.playerConnection();
        QCOMPARE(connection.host, QString("127.0.0.1"));
        QCOMPARE(connection.port, quint16(17655));
        QVERIFY(connection.token.isEmpty());
    }

    void playerConnectionPersistsIndependently()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString file = directory.filePath("player.conf");
        Settings settings(file, nullptr);
        settings.setAppearanceId("tide");
        settings.setThemeId("dark");
        settings.writeFileFormatAudioValue("flac", true);
        settings.writeFileFormatVideoValue("mp4", false);
        settings.writeStringSettings("Station/Name", "Студия");
        PlayerConnectionSettings connection;
        connection.host = "  player.local  ";
        connection.port = 65535;
        connection.token = QString(64, QLatin1Char('a')) + '\n';
        QVERIFY(settings.setPlayerConnection(connection));

        Settings reopened(file, nullptr);
        const PlayerConnectionSettings stored = reopened.playerConnection();
        QCOMPARE(stored.host, QString("player.local"));
        QCOMPARE(stored.port, quint16(65535));
        QCOMPARE(stored.token, QString(64, QLatin1Char('a')));
        QCOMPARE(reopened.appearanceId(), QString("tide"));
        QCOMPARE(reopened.themeId(), QString("dark"));
        QVERIFY(reopened.fileFormatsAudio().value("flac"));
        QVERIFY(!reopened.fileFormatsVideo().value("mp4"));

        const QSettings raw(file, QSettings::IniFormat);
        QCOMPARE(raw.value("Player/Host").toString(), stored.host);
        QCOMPARE(raw.value("Player/Port").toInt(), 65535);
        QCOMPARE(raw.value("Player/Token").toString(), stored.token);
        QCOMPARE(raw.value("Station/Name").toString(), QString("Студия"));

        connection.host = "::1";
        connection.port = 1;
        connection.token = QString(64, QLatin1Char('b'));
        QVERIFY(settings.setPlayerConnection(connection));
        QCOMPARE(reopened.playerConnection().host, connection.host);
        QCOMPARE(reopened.playerConnection().port, connection.port);
        QCOMPARE(reopened.playerConnection().token, connection.token);
    }

    void playerConnectionWriteFailureIsReported()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString file = directory.filePath("player.conf");
        Settings settings(file, nullptr);
        QVERIFY(QFileInfo(file).isFile());
        QVERIFY(QFile::remove(file));
        QVERIFY(QDir().mkpath(file));
        PlayerConnectionSettings connection;
        connection.token = QString(64, QLatin1Char('a'));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Не удалось сохранить настройки в .*"));
        QVERIFY(!settings.setPlayerConnection(connection));
        QVERIFY(QFileInfo(file).isDir());
    }

    void storedPlayerPortValidation_data()
    {
        QTest::addColumn<QString>("storedPort");
        QTest::addColumn<quint16>("expectedPort");
        QTest::newRow("minimum") << QString("1") << quint16(1);
        QTest::newRow("maximum") << QString("65535") << quint16(65535);
        QTest::newRow("zero") << QString("0") << quint16(17655);
        QTest::newRow("negative") << QString("-1") << quint16(17655);
        QTest::newRow("out-of-range") << QString("65536") << quint16(17655);
        QTest::newRow("integer-overflow") << QString("4294984951") << quint16(17655);
        QTest::newRow("not-a-number") << QString("invalid") << quint16(17655);
        QTest::newRow("fractional") << QString("17655.5") << quint16(17655);
    }

    void storedPlayerPortValidation()
    {
        QFETCH(QString, storedPort);
        QFETCH(quint16, expectedPort);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString file = directory.filePath("player.conf");
        Settings settings(file, nullptr);
        settings.writeStringSettings("Player/Port", storedPort);
        QCOMPARE(settings.playerConnection().port, expectedPort);
    }

    void appearanceSettingsPersistIndependently()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString file = directory.filePath("appearance.conf");
        Settings settings(file, nullptr);
        QCOMPARE(settings.appearanceId(), QString("tide-relief"));
        QCOMPARE(settings.themeId(), QString("denim"));
        settings.setThemeId("dark");
        QCOMPARE(settings.appearanceId(), QString("tide-relief"));
        settings.setAppearanceId("tide");
        QCOMPARE(settings.themeId(), QString("dark"));
        settings.setThemeId("not-a-theme");
        settings.setAppearanceId("not-an-appearance");
        Settings reopened(file, nullptr);
        QCOMPARE(reopened.appearanceId(), QString("tide"));
        QCOMPARE(reopened.themeId(), QString("dark"));
        QVERIFY(reopened.fileFormatsAudio().value("mp3"));
        QVERIFY(reopened.fileFormatsVideo().value("mp4"));

        QSettings external(file, QSettings::IniFormat);
        external.setValue("Appearance/style", "unknown");
        external.setValue("Appearance/theme", "unknown");
        external.sync();
        QCOMPARE(reopened.appearanceId(), QString("tide-relief"));
        QCOMPARE(reopened.themeId(), QString("denim"));
    }

    void configurationDoesNotDependOnWorkingDirectory()
    {
        const SettingsEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString originalWorkingDirectory = QDir::currentPath();
        const auto restoreDirectory = qScopeGuard([&] { QDir::setCurrent(originalWorkingDirectory); });
        const QString firstDirectory = directory.filePath("first");
        const QString secondDirectory = directory.filePath("second");
        QVERIFY(QDir().mkpath(firstDirectory));
        QVERIFY(QDir().mkpath(secondDirectory));
        QVERIFY(writeContents(QDir(firstDirectory).filePath("MediaBoxManager.conf"), "source=working-directory\n"));
        QVERIFY(QDir::setCurrent(firstDirectory));
        const QString selected = Settings::configurationFilePath();
        QCOMPARE(selected, managerConfigurationFile());
        QVERIFY(!QFileInfo::exists(selected));
        Settings settings(selected, nullptr);
        settings.setThemeId("dark");
        QVERIFY(QDir::setCurrent(secondDirectory));
        QCOMPARE(Settings::configurationFilePath(), selected);
        Settings reopened(selected, nullptr);
        QCOMPARE(reopened.themeId(), QString("dark"));
        QVERIFY(!QFileInfo::exists(QDir(secondDirectory).filePath("MediaBoxManager.conf")));
    }

    void previewConfigurationOverrideIsPreserved()
    {
        const SettingsEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString previewFile = directory.filePath("preview.conf");
        QCoreApplication::instance()->setProperty("restylePreviewSettings", previewFile);
        QCOMPARE(Settings::configurationFilePath(), previewFile);
        QVERIFY(!QFileInfo::exists(managerConfigurationFile()));
    }

    void configurationDirectoryFailure()
    {
        const SettingsEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString configurationDirectory = QFileInfo(managerConfigurationFile()).absolutePath();
        const QString source = directory.filePath("MediaBoxManager.conf");
        QVERIFY(writeContents(source, "source=local\n"));
        QVERIFY(writeContents(configurationDirectory, "blocked\n"));
        QVERIFY_EXCEPTION_THROWN(Settings::configurationFilePath(), std::runtime_error);
        QVERIFY(QFileInfo(configurationDirectory).isFile());
        QVERIFY(QFileInfo(source).isFile());
    }

    void configurationPathIsDirectory()
    {
        const SettingsEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString selected = managerConfigurationFile();
        QVERIFY(QDir().mkpath(selected));
        QVERIFY_EXCEPTION_THROWN(Settings::configurationFilePath(), std::runtime_error);
        QVERIFY(QFileInfo(selected).isDir());
    }

    void configurationWriteFailureIsReported()
    {
        const SettingsEnvironment environment;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString selected = Settings::configurationFilePath();
        Settings settings(selected, nullptr);
        QVERIFY(QFileInfo(selected).isFile());
        QVERIFY(QFile::remove(selected));
        QVERIFY(QDir().mkpath(selected));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Не удалось сохранить настройки в .*"));
        settings.writeStringSettings("blocked-write", "value");
        QVERIFY(QFileInfo(selected).isDir());
    }

    void logUsesManagerDataDirectory()
    {
        const SettingsEnvironment environment;
        const QString logDirectory = MediaBox::StoragePaths::dataDirectory(
                MediaBox::StoragePaths::Application::Manager);
        QVERIFY(QDir::isAbsolutePath(logDirectory));
        QVERIFY(!QDir(logDirectory).exists());
        const QString marker = "manager data directory log test";
        { BoxLog log(marker); }
        QFile file(QDir(logDirectory).absoluteFilePath("MediaBoxManager.log"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains(marker.toUtf8()));
    }
};

QTEST_GUILESS_MAIN(SettingsTest)
#include "tst_settings.moc"
