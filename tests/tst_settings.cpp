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

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace {
class SettingsEnvironment final
{
public:
    SettingsEnvironment(const QString &programData, const QString &programFiles,
                        const QByteArray &programFilesVariable = "ProgramFiles",
                        const QString &applicationName = "MediaBoxManager")
        : originalName(QCoreApplication::applicationName())
    {
        for (int i = 0; i < 4; ++i) {
            originalValues[i] = qgetenv(variables[i]);
            originalVariablesSet[i] = qEnvironmentVariableIsSet(variables[i]);
            qunsetenv(variables[i]);
        }
        QCoreApplication::setApplicationName(applicationName);
        qputenv("ProgramData", QDir::toNativeSeparators(programData).toUtf8());
        qputenv(programFilesVariable.constData(), QDir::toNativeSeparators(programFiles).toUtf8());
    }

    ~SettingsEnvironment()
    {
        for (int i = 0; i < 4; ++i) {
            if (originalVariablesSet[i])
                qputenv(variables[i], originalValues[i]);
            else
                qunsetenv(variables[i]);
        }
        QCoreApplication::setApplicationName(originalName);
    }

private:
    const char *variables[4] = {"ProgramW6432", "ProgramFiles", "ProgramFiles(x86)", "ProgramData"};
    QByteArray originalValues[4];
    bool originalVariablesSet[4] = {};
    QString originalName;
};

bool writeContents(const QString &path, const QByteArray &contents)
{
    if (!QFileInfo(path).dir().mkpath("."))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}
}
#endif

class SettingsTest final : public QObject
{
    Q_OBJECT

private slots:
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

    void configurationFileSelection_data()
    {
        QTest::addColumn<QString>("applicationName");
        QTest::addColumn<bool>("legacyExists");
        QTest::addColumn<bool>("preferredExists");
        QTest::addColumn<QString>("selectedName");

        QTest::newRow("new-install") << QString("MediaBoxManager") << false << false
                                     << QString("MediaBoxManager.conf");
        QTest::newRow("legacy-settings") << QString("MediaBoxManager") << true << false
                                         << QString("lampbox.conf");
        QTest::newRow("new-settings-preferred") << QString("MediaBoxManager") << true << true
                                                << QString("MediaBoxManager.conf");
        QTest::newRow("new-settings-only") << QString("MediaBoxManager") << false << true
                                           << QString("MediaBoxManager.conf");
        QTest::newRow("other-application") << QString("SettingsTestApplication") << true << false
                                           << QString("SettingsTestApplication.conf");
    }

    void configurationFileSelection()
    {
        QFETCH(QString, applicationName);
        QFETCH(bool, legacyExists);
        QFETCH(bool, preferredExists);
        QFETCH(QString, selectedName);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString originalApplicationName = QCoreApplication::applicationName();
        const bool originalTestMode = QStandardPaths::isTestModeEnabled();
        const auto restoreApplication = qScopeGuard([&] {
            QCoreApplication::setApplicationName(originalApplicationName);
            QStandardPaths::setTestModeEnabled(originalTestMode);
        });
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName(applicationName);

        const QString legacyPath = directory.filePath("lampbox.conf");
        const QString preferredPath = directory.filePath(applicationName + ".conf");
        const QByteArray legacyContents("source=legacy\n[FileFormats]\nAudioFormats/flac=true\n");
        const QByteArray preferredContents("source=current\n[FileFormats]\nAudioFormats/ogg=true\n");
        if (legacyExists) {
            QFile file(legacyPath);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(legacyContents), qint64(legacyContents.size()));
        }
        if (preferredExists) {
            QFile file(preferredPath);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(preferredContents), qint64(preferredContents.size()));
        }

        const QString selectedPath = Settings::configurationFilePath(directory.path());
        QCOMPARE(selectedPath, directory.filePath(selectedName));
        if (QFileInfo::exists(selectedPath)) {
            const QSettings settings(selectedPath, QSettings::IniFormat);
            QCOMPARE(settings.value("source").toString(),
                     selectedName == "lampbox.conf" ? QString("legacy") : QString("current"));
        }
        QCOMPARE(QFileInfo::exists(legacyPath), legacyExists);
        QCOMPARE(QFileInfo::exists(preferredPath), preferredExists);
        if (legacyExists) {
            QFile file(legacyPath);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), legacyContents);
        }
        if (preferredExists) {
            QFile file(preferredPath);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), preferredContents);
        }
    }

#ifdef Q_OS_WIN
    void installedConfiguration_data()
    {
        QTest::addColumn<QByteArray>("environmentVariable");
        QTest::addColumn<int>("layout");
        QTest::addColumn<bool>("localNewExists");
        QTest::addColumn<bool>("localLegacyExists");
        QTest::addColumn<bool>("userExists");
        QTest::addColumn<bool>("sharedExists");
        QTest::addColumn<QByteArray>("expectedContents");

        QTest::newRow("programw6432-legacy") << QByteArray("ProgramW6432") << 0 << false << true << false << false << QByteArray("source=legacy\n");
        QTest::newRow("programfiles-new-install") << QByteArray("ProgramFiles") << 0 << false << false << false << false << QByteArray();
        QTest::newRow("programfiles-x86-new-preferred") << QByteArray("ProgramFiles(x86)") << 0 << true << true << false << false << QByteArray("source=current\n");
        QTest::newRow("previous-user-file-preferred") << QByteArray("ProgramW6432") << 0 << true << true << true << false << QByteArray("source=user\n");
        QTest::newRow("existing-shared-file-preferred") << QByteArray("ProgramFiles") << 0 << true << true << true << true << QByteArray("source=shared\n");
        QTest::newRow("existing-shared-file-only") << QByteArray("ProgramFiles") << 0 << false << false << false << true << QByteArray("source=shared\n");
        QTest::newRow("normalized-case-insensitive-path") << QByteArray("ProgramFiles") << 2 << false << true << false << false << QByteArray("source=legacy\n");
        QTest::newRow("root-directory") << QByteArray("ProgramFiles") << 3 << false << true << false << false << QByteArray("source=legacy\n");
        QTest::newRow("portable-sibling-prefix") << QByteArray("ProgramFiles") << 1 << false << true << false << false << QByteArray("source=legacy\n");
        QTest::newRow("other-application") << QByteArray("ProgramFiles") << 4 << false << true << false << false << QByteArray();
    }

    void installedConfiguration()
    {
        QFETCH(QByteArray, environmentVariable);
        QFETCH(int, layout);
        QFETCH(bool, localNewExists);
        QFETCH(bool, localLegacyExists);
        QFETCH(bool, userExists);
        QFETCH(bool, sharedExists);
        QFETCH(QByteArray, expectedContents);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString programFiles = directory.filePath("Program Files");
        const QString programData = directory.filePath("ProgramData");
        const QString userDirectory = directory.filePath("AppConfigLocation");
        const QString applicationName = layout == 4 ? "InstalledSettingsTestApplication" : "MediaBoxManager";
        const SettingsEnvironment environment(programData, programFiles + "/.", environmentVariable, applicationName);
        QString applicationDirectory = programFiles + "/MediaBox/bin";
        if (layout == 1)
            applicationDirectory = programFiles + "Portable/MediaBox/bin";
        else if (layout == 2)
            applicationDirectory = QDir::toNativeSeparators(programFiles.toUpper() + "/Unused/../MediaBox/bin");
        else if (layout == 3)
            applicationDirectory = programFiles;
        const QDir applicationDir(applicationDirectory);
        QVERIFY(applicationDir.mkpath("."));
        const QString localNew = applicationDir.absoluteFilePath(applicationName + ".conf");
        const QString localLegacy = applicationDir.absoluteFilePath("lampbox.conf");
        const QString userFile = QDir(userDirectory).absoluteFilePath("MediaBoxManager.conf");
        const QString sharedDirectory = QDir(programData).absoluteFilePath("MediaBox");
        const QString sharedFile = QDir(sharedDirectory).absoluteFilePath("MediaBoxManager.conf");
        const QByteArray legacyContents("source=legacy\n");
        const QByteArray localContents("source=current\n");
        const QByteArray userContents("source=user\n");
        const QByteArray sharedContents("source=shared\n");
        if (localLegacyExists)
            QVERIFY(writeContents(localLegacy, legacyContents));
        if (localNewExists)
            QVERIFY(writeContents(localNew, localContents));
        if (userExists)
            QVERIFY(writeContents(userFile, userContents));
        if (sharedExists)
            QVERIFY(writeContents(sharedFile, sharedContents));

        const QString selected = Settings::configurationFilePath(applicationDirectory, userDirectory);
        const bool installed = layout != 1 && layout != 4;
        if (installed) {
            QCOMPARE(selected, sharedFile);
            QVERIFY(QDir(sharedDirectory).exists());
            if (expectedContents.isEmpty()) {
                QVERIFY(!QFileInfo::exists(sharedFile));
            } else {
                QFile file(sharedFile);
                QVERIFY(file.open(QIODevice::ReadOnly));
                QCOMPARE(file.readAll(), expectedContents);
            }
            {
                Settings settings(selected, nullptr);
                settings.writeStringSettings("shared/value", "persistent");
                settings.writeFileFormatAudioValue("flac", true);
                settings.writeFileFormatVideoValue("mp4", false);
            }
            QCOMPARE(Settings::configurationFilePath(applicationDirectory, userDirectory), sharedFile);
            const QSettings settings(sharedFile, QSettings::IniFormat);
            QCOMPARE(settings.value("shared/value").toString(), QString("persistent"));
            {
                Settings reopened(selected, nullptr);
                QVERIFY(reopened.fileFormatsAudio().value("flac"));
                QVERIFY(!reopened.fileFormatsVideo().value("mp4", true));
            }
        } else {
            QCOMPARE(selected, layout == 4 ? localNew : localLegacy);
            QVERIFY(!QDir(programData).exists());
        }
        QCOMPARE(QFileInfo(localNew).isFile(), localNewExists);
        QCOMPARE(QFileInfo(localLegacy).isFile(), localLegacyExists);
        QCOMPARE(QFileInfo(userFile).isFile(), userExists);
        for (const auto &source : {qMakePair(localNew, localContents),
                                   qMakePair(localLegacy, legacyContents),
                                   qMakePair(userFile, userContents)}) {
            if (!QFileInfo(source.first).isFile())
                continue;
            QFile file(source.first);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), source.second);
        }
    }

    void migrationSkipsDirectories_data()
    {
        QTest::addColumn<bool>("userDirectoryInsteadOfFile");
        QTest::addColumn<bool>("localDirectoryInsteadOfFile");
        QTest::newRow("user-source-is-directory") << true << false;
        QTest::newRow("local-source-is-directory") << false << true;
        QTest::newRow("both-sources-are-directories") << true << true;
    }

    void migrationSkipsDirectories()
    {
        QFETCH(bool, userDirectoryInsteadOfFile);
        QFETCH(bool, localDirectoryInsteadOfFile);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString programFiles = directory.filePath("Program Files");
        const QString programData = directory.filePath("ProgramData");
        const QString userDirectory = directory.filePath("AppConfigLocation");
        const QString applicationDirectory = programFiles + "/MediaBox/bin";
        const SettingsEnvironment environment(programData, programFiles);
        const QString userFile = QDir(userDirectory).filePath("MediaBoxManager.conf");
        const QString localFile = QDir(applicationDirectory).filePath("MediaBoxManager.conf");
        const QString legacyFile = QDir(applicationDirectory).filePath("lampbox.conf");
        const QByteArray legacyContents("source=legacy\n");
        QVERIFY(writeContents(legacyFile, legacyContents));
        if (userDirectoryInsteadOfFile)
            QVERIFY(QDir().mkpath(userFile));
        if (localDirectoryInsteadOfFile)
            QVERIFY(QDir().mkpath(localFile));
        const QString selected = Settings::configurationFilePath(applicationDirectory, userDirectory);
        QCOMPARE(selected, QDir(programData).absoluteFilePath("MediaBox/MediaBoxManager.conf"));
        QFile file(selected);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), legacyContents);
        QCOMPARE(QFileInfo(userFile).isDir(), userDirectoryInsteadOfFile);
        QCOMPARE(QFileInfo(localFile).isDir(), localDirectoryInsteadOfFile);
        QVERIFY(QFileInfo(legacyFile).isFile());
    }

    void installedConfigurationDirectoryFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString programFiles = directory.filePath("Program Files");
        const QString programData = directory.filePath("ProgramData");
        const QString userDirectory = directory.filePath("AppConfigLocation");
        const QString applicationDirectory = programFiles + "/MediaBox/bin";
        const SettingsEnvironment environment(programData, programFiles);
        const QString source = QDir(applicationDirectory).filePath("MediaBoxManager.conf");
        QVERIFY(writeContents(source, "source=current\n"));
        QVERIFY(writeContents(programData, "blocked\n"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Не удалось создать каталог настроек: .*"));
        QCOMPARE(Settings::configurationFilePath(applicationDirectory, userDirectory),
                 QDir(programData).absoluteFilePath("MediaBox/MediaBoxManager.conf"));
        QVERIFY(QFileInfo(programData).isFile());
        QVERIFY(QFileInfo(source).isFile());
    }

    void installedConfigurationCopyFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString programFiles = directory.filePath("Program Files");
        const QString programData = directory.filePath("ProgramData");
        const QString userDirectory = directory.filePath("AppConfigLocation");
        const QString applicationDirectory = programFiles + "/MediaBox/bin";
        const SettingsEnvironment environment(programData, programFiles);
        const QString source = QDir(applicationDirectory).filePath("MediaBoxManager.conf");
        const QByteArray contents("source=locked\n");
        QVERIFY(writeContents(source, contents));
        HANDLE sourceHandle = CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(source).utf16()),
                                          GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(sourceHandle != INVALID_HANDLE_VALUE);
        const auto unlockSource = qScopeGuard([&] {
            if (sourceHandle != INVALID_HANDLE_VALUE)
                CloseHandle(sourceHandle);
        });
        QVERIFY(QFileInfo(source).isFile());
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Не удалось скопировать настройки из .*"));
        const QString selected = Settings::configurationFilePath(applicationDirectory, userDirectory);
        QCOMPARE(selected, QDir(programData).absoluteFilePath("MediaBox/MediaBoxManager.conf"));
        QVERIFY(!QFileInfo::exists(selected));
        CloseHandle(sourceHandle);
        sourceHandle = INVALID_HANDLE_VALUE;
        QFile file(source);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), contents);
    }

    void sharedConfigurationPathIsDirectory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString programFiles = directory.filePath("Program Files");
        const QString programData = directory.filePath("ProgramData");
        const QString userDirectory = directory.filePath("AppConfigLocation");
        const QString applicationDirectory = programFiles + "/MediaBox/bin";
        const SettingsEnvironment environment(programData, programFiles);
        const QString sharedFile = QDir(programData).absoluteFilePath("MediaBox/MediaBoxManager.conf");
        QVERIFY(QDir().mkpath(sharedFile));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Путь настроек не является файлом: .*"));
        QCOMPARE(Settings::configurationFilePath(applicationDirectory, userDirectory), sharedFile);
        QVERIFY(QFileInfo(sharedFile).isDir());
    }

    void sharedConfigurationWriteFailureIsReported()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString programFiles = directory.filePath("Program Files");
        const QString programData = directory.filePath("ProgramData");
        const QString userDirectory = directory.filePath("AppConfigLocation");
        const QString applicationDirectory = programFiles + "/MediaBox/bin";
        const SettingsEnvironment environment(programData, programFiles);
        const QString selected = Settings::configurationFilePath(applicationDirectory, userDirectory);
        Settings settings(selected, nullptr);
        QVERIFY(QFileInfo(selected).isFile());
        QVERIFY(QFile::remove(selected));
        QVERIFY(QDir().mkpath(selected));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("^Не удалось сохранить настройки в .*"));
        settings.writeStringSettings("blocked-write", "value");
        QVERIFY(QFileInfo(selected).isDir());
    }
#endif
    void logUsesUserDataDirectory()
    {
        const QString originalName = QCoreApplication::applicationName();
        const QString originalOrganization = QCoreApplication::organizationName();
        const bool originalTestMode = QStandardPaths::isTestModeEnabled();
        const QString testOrganization = "MediaBoxManagerLogTest_"
                + QUuid::createUuid().toString(QUuid::Id128);
        QString logDirectory;
        bool mayCleanLogDirectory = false;
        const auto restoreApplication = qScopeGuard([&] {
            if (mayCleanLogDirectory && logDirectory.contains(testOrganization)) {
                QDir(logDirectory).removeRecursively();
                QDir().rmdir(QFileInfo(logDirectory).dir().absolutePath());
            }
            QCoreApplication::setApplicationName(originalName);
            QCoreApplication::setOrganizationName(originalOrganization);
            QStandardPaths::setTestModeEnabled(originalTestMode);
        });
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(testOrganization);
        QCoreApplication::setApplicationName("MediaBoxManager");
        logDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QVERIFY(QDir::isAbsolutePath(logDirectory));
        QVERIFY(logDirectory.contains(testOrganization));
        QVERIFY(!QDir(logDirectory).exists());
        mayCleanLogDirectory = true;
        const QString marker = "ordinary user log test";
        { BoxLog log(marker); }
        QFile file(QDir(logDirectory).absoluteFilePath("MediaBoxManager.log"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains(marker.toUtf8()));
    }
};

QTEST_GUILESS_MAIN(SettingsTest)
#include "tst_settings.moc"
