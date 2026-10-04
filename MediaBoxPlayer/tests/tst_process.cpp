#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

class ProcessTests final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // Windows may route Qt diagnostics to the debugger; child-process
        // assertions need the same captured stderr as other platforms.
        qputenv("QT_FORCE_STDERR_LOGGING", "1");
    }

    void managedRejectsInvalidLaunch_data()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<QByteArray>("token");
        QTest::newRow("missing-token") << QStringList{} << QByteArray{};
        QTest::newRow("short-token") << QStringList{} << QByteArray(63, 'a');
        QTest::newRow("uppercase-token") << QStringList{} << QByteArray(64, 'A');
        QTest::newRow("non-hex-token") << QStringList{} << QByteArray(64, 'z');
        QTest::newRow("whitespace-token") << QStringList{} << (QByteArray(64, 'a') + '\n');
        QTest::newRow("public-listen") << QStringList{"--listen", "0.0.0.0"} << QByteArray(64, 'a');
        QTest::newRow("public-ipv6-listen") << QStringList{"--listen", "::"} << QByteArray(64, 'a');
        QTest::newRow("remote-listen") << QStringList{"--listen", "192.0.2.1"} << QByteArray(64, 'a');
#ifdef Q_OS_WIN
        QTest::newRow("windows-service-conflict") << QStringList{"--service"} << QByteArray(64, 'a');
#endif
    }

    void managedRejectsInvalidLaunch()
    {
        QFETCH(QStringList, arguments);
        QFETCH(QByteArray, token);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert("MEDIABOXPLAYER_CONTROL_TOKEN", QString::fromLatin1(token));
        QProcess process;
        process.setProcessEnvironment(environment);
        arguments.prepend(directory.filePath("uncreated"));
        arguments.prepend("--data-dir");
        arguments.prepend("--managed");
        process.start(QStringLiteral(PLAYER_EXECUTABLE), arguments);
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 2);
        QVERIFY(!QFile::exists(directory.filePath("uncreated")));
        const QByteArray error = process.readAllStandardError();
        QVERIFY(!error.contains("Cannot connect MediaBoxPlayer to Windows Service Control Manager"));
        if (!token.isEmpty())
            QVERIFY(!error.contains(token));
    }

    void managedRequiresExplicitDirectory()
    {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert("MEDIABOXPLAYER_CONTROL_TOKEN", QString(64, 'a'));
        QProcess process;
        process.setProcessEnvironment(environment);
        process.start(QStringLiteral(PLAYER_EXECUTABLE), {"--managed"});
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 2);
        QVERIFY(process.readAllStandardError().contains("explicit --data-dir"));
    }

    void rejectsInvalidPort()
    {
        QProcess process;
        process.start(QStringLiteral(PLAYER_EXECUTABLE), {"--port", "65536"});
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 2);
    }

#ifdef Q_OS_LINUX
    void defaultDirectoryIsSystemWide()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert("XDG_CONFIG_HOME", directory.filePath("config"));
        environment.insert("XDG_DATA_HOME", directory.filePath("data"));
        QProcess process;
        process.setProcessEnvironment(environment);
        process.start(QStringLiteral(PLAYER_EXECUTABLE), {"--help"});
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        QVERIFY(process.readAllStandardOutput().contains("/etc/mediabox/mediaboxplayer"));
        QVERIFY(!QFile::exists(directory.filePath("config")));
        QVERIFY(!QFile::exists(directory.filePath("data")));
    }
#endif

#ifdef Q_OS_WIN
    void storageDirectory_data()
    {
        QTest::addColumn<bool>("hasLegacyToken");
        QTest::addColumn<bool>("hasCurrentToken");
        QTest::addColumn<bool>("explicitDirectory");
        QTest::addColumn<bool>("explicitDefaultDirectory");
        QTest::newRow("default-migrates-token") << true << false << false << false;
        QTest::newRow("default-preserves-current-token") << true << true << false << false;
        QTest::newRow("explicit-directory-does-not-migrate") << true << false << true << false;
        QTest::newRow("explicit-default-directory-does-not-migrate") << true << false << true << true;
    }

    void storageDirectory()
    {
        QFETCH(bool, hasLegacyToken);
        QFETCH(bool, hasCurrentToken);
        QFETCH(bool, explicitDirectory);
        QFETCH(bool, explicitDefaultDirectory);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString programData = directory.filePath("ProgramData");
        const QString defaultDirectory = QDir(programData).filePath("MediaBox/MediaBoxPlayer");
        const QString selectedDirectory = explicitDirectory && !explicitDefaultDirectory
            ? directory.filePath("custom") : defaultDirectory;
        const QString legacyTokenPath = QDir(programData).filePath("MediaBox/Player/control.token");
        const QString currentTokenPath = QDir(selectedDirectory).filePath("control.token");
        const QByteArray legacyToken = QByteArray(64, 'a') + '\n';
        const QByteArray currentToken = QByteArray(64, 'b') + '\n';
        const auto writeToken = [](const QString &path, const QByteArray &token) {
            if (!QDir().mkpath(QFileInfo(path).absolutePath()))
                return false;
            QFile file(path);
            return file.open(QIODevice::WriteOnly)
                && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
                && file.write(token) == token.size();
        };
        if (hasLegacyToken)
            QVERIFY(writeToken(legacyTokenPath, legacyToken));
        if (hasCurrentToken)
            QVERIFY(writeToken(currentTokenPath, currentToken));

        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const auto port = reservation.serverPort();
        reservation.close();
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert("ProgramData", programData);
        QProcess process;
        process.setProcessEnvironment(environment);
        QStringList arguments{"--port", QString::number(port)};
        if (explicitDirectory)
            arguments.append({"--data-dir", selectedDirectory});
        process.start(QStringLiteral(PLAYER_EXECUTABLE), arguments);
        QVERIFY(process.waitForStarted(5000));
        QTcpSocket socket;
        const auto connected = [&] {
            if (socket.state() == QAbstractSocket::ConnectedState)
                return true;
            socket.abort();
            socket.connectToHost(QHostAddress::LocalHost, port);
            return socket.waitForConnected(50);
        };
        QTRY_VERIFY_WITH_TIMEOUT(connected(), 5000);
        QFile token(currentTokenPath);
        QVERIFY(token.open(QIODevice::ReadOnly));
        const QByteArray actualToken = token.readAll();
        QCOMPARE(actualToken.trimmed().size(), 64);
        if (hasCurrentToken)
            QCOMPARE(actualToken, currentToken);
        else if (hasLegacyToken && !explicitDirectory)
            QCOMPARE(actualToken, legacyToken);
        else
            QVERIFY(actualToken != legacyToken);
        if (hasLegacyToken) {
            QFile legacyFile(legacyTokenPath);
            QVERIFY(legacyFile.open(QIODevice::ReadOnly));
            QCOMPARE(legacyFile.readAll(), legacyToken);
        }
        if (explicitDirectory && !explicitDefaultDirectory)
            QVERIFY(!QFile::exists(QDir(defaultDirectory).filePath("control.token")));
        QVERIFY(QFile::exists(QDir(selectedDirectory).filePath("player.lock")));

        process.kill(); // SCM shutdown is covered when testing an installed Windows service.
        QVERIFY(process.waitForFinished(5000));
        QVERIFY(!process.readAllStandardError().contains(actualToken.trimmed()));
    }
#endif

    void serviceAcceptsCommandsAndStopsCleanly_data()
    {
        QTest::addColumn<bool>("managed");
        QTest::addColumn<bool>("existingTokenFile");
        QTest::newRow("background") << false << false;
        QTest::newRow("managed") << true << false;
        QTest::newRow("managed-preserves-standalone-token") << true << true;
    }

    void serviceAcceptsCommandsAndStopsCleanly()
    {
        QFETCH(bool, managed);
        QFETCH(bool, existingTokenFile);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray managedToken(64, 'c');
        const QByteArray standaloneToken(64, 'd');
        if (existingTokenFile) {
            QFile tokenFile(directory.filePath("control.token"));
            QVERIFY(tokenFile.open(QIODevice::WriteOnly));
            QCOMPARE(tokenFile.write(standaloneToken + '\n'), 65);
        }
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const auto port = reservation.serverPort();
        reservation.close();
        QProcess process;
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        // An inherited env token must never override a standalone token.
        environment.insert("MEDIABOXPLAYER_CONTROL_TOKEN", QString::fromLatin1(managedToken));
        process.setProcessEnvironment(environment);
        QStringList arguments{"--data-dir", directory.path(), "--port", QString::number(port)};
        if (managed)
            arguments.prepend("--managed");
        process.start(QStringLiteral(PLAYER_EXECUTABLE), arguments);
        QVERIFY(process.waitForStarted(5000));
        QTcpSocket socket;
        const auto connected = [&] {
            if (socket.state() == QAbstractSocket::ConnectedState)
                return true;
            socket.abort();
            socket.connectToHost(QHostAddress::LocalHost, port);
            return socket.waitForConnected(50);
        };
        QTRY_VERIFY_WITH_TIMEOUT(connected(), 5000);
        QByteArray controlToken = managedToken;
        if (!managed || existingTokenFile) {
            QFile token(directory.filePath("control.token"));
            QVERIFY(token.open(QIODevice::ReadOnly));
            const QByteArray fileToken = token.readAll().trimmed();
            if (managed) {
                QCOMPARE(fileToken, standaloneToken);
            } else {
                QCOMPARE(fileToken.size(), 64);
                QVERIFY(fileToken != managedToken);
                controlToken = fileToken;
            }
        } else {
            QVERIFY(!QFile::exists(directory.filePath("control.token")));
        }
        const QJsonObject command{{"protocolVersion", 1}, {"token", QString::fromLatin1(controlToken)},
                                  {"command", "status"}, {"id", "service-test"}};
        socket.write(QJsonDocument(command).toJson(QJsonDocument::Compact) + '\n');
        QVERIFY(socket.waitForReadyRead(5000));
        QTRY_VERIFY(socket.canReadLine());
        const auto response = QJsonDocument::fromJson(socket.readLine()).object();
        QVERIFY(response.value("ok").toBool());
        QCOMPARE(response.value("id").toString(), QStringLiteral("service-test"));
        QCOMPARE(response.value("status").toObject().value("state").toString(), QStringLiteral("stopped"));
        QVERIFY(!response.value("status").toObject().value("playbackRequested").toBool());
        QVERIFY(response.value("status").toObject().value("queue").toArray().isEmpty());

        if (managed && existingTokenFile) {
            QJsonObject wrongToken = command;
            wrongToken.insert("token", QString::fromLatin1(standaloneToken));
            socket.write(QJsonDocument(wrongToken).toJson(QJsonDocument::Compact) + '\n');
            QVERIFY(socket.waitForReadyRead(5000));
            QTRY_VERIFY(socket.canReadLine());
            const auto rejected = QJsonDocument::fromJson(socket.readLine()).object();
            QVERIFY(!rejected.value("ok").toBool());
            QCOMPARE(rejected.value("error").toObject().value("code").toString(), QStringLiteral("unauthorized"));
        }

        QJsonObject volumeCommand = command;
        volumeCommand.insert("command", "volume");
        volumeCommand.insert("value", 37);
        socket.write(QJsonDocument(volumeCommand).toJson(QJsonDocument::Compact) + '\n');
        QVERIFY(socket.waitForReadyRead(5000));
        QTRY_VERIFY(socket.canReadLine());
        const auto volumeResponse = QJsonDocument::fromJson(socket.readLine()).object();
        QVERIFY(volumeResponse.value("ok").toBool());
        QCOMPARE(volumeResponse.value("status").toObject().value("volumePercent").toInt(), 37);

        // The data-directory lock applies even when a second process chooses another port.
        QProcess duplicate;
        duplicate.start(QStringLiteral(PLAYER_EXECUTABLE),
                        {"--data-dir", directory.path(), "--port", port == 17655 ? "17656" : "17655"});
        QVERIFY(duplicate.waitForFinished(5000));
        QCOMPARE(duplicate.exitCode(), 1);
        QCOMPARE(process.state(), QProcess::Running);

#ifdef Q_OS_UNIX
        process.terminate(); // SIGTERM must reach the event loop and release the lock.
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        QVERIFY(!QFile::exists(directory.filePath("player.lock")));
#else
        // Windows SCM stop is tested on an installed service, not a console subprocess.
        process.kill();
        QVERIFY(process.waitForFinished(5000));
#endif
        QVERIFY(!process.readAllStandardError().contains(controlToken));
    }
};

QTEST_GUILESS_MAIN(ProcessTests)
#include "tst_process.moc"
