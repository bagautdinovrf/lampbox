#include <QFile>
#include <QCryptographicHash>
#include <QDir>
#include <QUuid>
#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QJsonObject writePublication(const QString &root, int revision)
{
    const QString publicationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject document{{"format", "mediabox.schedule"}, {"schemaVersion", 1},
        {"scheduleId", "00000000-0000-4000-8000-000000000001"},
        {"stationId", "00000000-0000-4000-8000-000000000002"}, {"publicationId", publicationId},
        {"revision", revision}, {"publishedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}, {"timeZone", "UTC"},
        {"validity", QJsonObject{{"from", QDate::currentDate().addDays(-1).toString(Qt::ISODate)},
                                 {"until", QDate::currentDate().addDays(30).toString(Qt::ISODate)}}},
        {"musicTransition", "finish_track"}, {"timeResolution", QJsonObject{{"gap", "skip"}, {"overlap", "first"}}},
        {"requiredCapabilities", QJsonArray{"calendar.v1", "media.video.v1"}},
        {"fallback", QJsonObject{{"source", QJsonObject{{"type", "silence"}}}, {"volumePercent", 0}}},
        {"assets", QJsonArray{QJsonObject{{"id", "00000000-0000-4000-8000-000000000003"},
                                        {"path", "Левый.wav"}, {"mediaType", "video"}}}},
        {"playlists", QJsonArray{}}, {"calendars", QJsonArray{}}, {"dayTemplates", QJsonArray{}},
        {"baseRules", QJsonArray{}}, {"mixRules", QJsonArray{}}, {"eventRules", QJsonArray{}}};
    const QByteArray bytes = QJsonDocument(document).toJson();
    const QString relative = "snapshots/" + publicationId + ".json";
    const QJsonObject active{{"format", "mediabox.active"}, {"schemaVersion", 1},
        {"scheduleId", document.value("scheduleId")}, {"stationId", document.value("stationId")},
        {"publicationId", publicationId}, {"revision", revision}, {"snapshotPath", relative},
        {"sha256", QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())}};
    if (!QDir().mkpath(QDir(root).filePath("snapshots"))) return {};
    QFile snapshot(QDir(root).filePath(relative));
    if (!snapshot.open(QIODevice::WriteOnly) || snapshot.write(bytes) != bytes.size()) return {};
    snapshot.close();
    QFile pointer(QDir(root).filePath("active.json"));
    const QByteArray pointerBytes = QJsonDocument(active).toJson();
    if (!pointer.open(QIODevice::WriteOnly) || pointer.write(pointerBytes) != pointerBytes.size()) return {};
    return {{"command", "loadPublication"}, {"windowId", "left"},
            {"activePath", pointer.fileName()}, {"contentRoot", root}, {"autoplay", false}};
}

class VideoProcess {
public:
    ~VideoProcess() { stop(); }

    bool start(const QString &directory, bool managed = false)
    {
        QTcpServer reservation;
        if (!reservation.listen(QHostAddress::LocalHost, 0))
            return false;
        port = reservation.serverPort();
        reservation.close();
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        // Standalone mode must ignore an inherited token environment variable.
        environment.insert("MEDIABOXVPLAYER_CONTROL_TOKEN", QString(64, 'c'));
        process.setProcessEnvironment(environment);
        QStringList arguments{"--data-dir", directory, "--host", "127.0.0.1",
                              "--port", QString::number(port)};
        if (managed)
            arguments.prepend("--managed");
        process.start(QStringLiteral(VPLAYER_EXECUTABLE), arguments);
        return process.waitForStarted(5000);
    }

    bool connectSocket()
    {
        if (socket.state() == QAbstractSocket::ConnectedState)
            return true;
        socket.abort();
        socket.connectToHost(QHostAddress::LocalHost, port);
        return socket.waitForConnected(50);
    }

    QJsonObject send(QJsonObject request, const QByteArray &token)
    {
        const QString id = QString::number(++sequence);
        request.insert("id", id);
        request.insert("protocolVersion", 1);
        request.insert("token", QString::fromLatin1(token));
        socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
        socket.flush();
        QElapsedTimer timer;
        timer.start();
        while (!socket.canReadLine() && timer.elapsed() < 5000)
            socket.waitForReadyRead(100);
        const auto reply = QJsonDocument::fromJson(socket.readLine()).object();
        if (reply.value("id").toString() != id || reply.value("protocolVersion").toInt() != 1)
            return {};
        return reply;
    }

    bool stop()
    {
        socket.abort();
        if (process.state() == QProcess::NotRunning)
            return true;
#ifdef Q_OS_WIN
        // WM_CLOSE only hides video windows; it deliberately keeps the API alive.
        // This test's isolated process has no Windows service stop channel.
        process.kill();
        process.waitForFinished(5000);
        return false;
#else
        process.terminate();
        const bool graceful = process.waitForFinished(5000);
        if (!graceful) {
            process.kill();
            process.waitForFinished(5000);
        }
        return graceful;
#endif
    }

    QProcess process;
    QTcpSocket socket;
    quint16 port = 0;
    int sequence = 0;
};

QJsonObject window(const QJsonObject &reply, const QString &id)
{
    for (const auto value : reply.value("status").toObject().value("windows").toArray()) {
        const auto object = value.toObject();
        if (object.value("id").toString() == id)
            return object;
    }
    return {};
}
}

class VideoProcessTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
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
        QTest::newRow("public-host") << QStringList{"--host", "0.0.0.0"} << QByteArray(64, 'a');
        QTest::newRow("public-ipv6-host") << QStringList{"--host", "::"} << QByteArray(64, 'a');
        QTest::newRow("remote-host") << QStringList{"--host", "192.0.2.1"} << QByteArray(64, 'a');
    }

    void managedRejectsInvalidLaunch()
    {
        QFETCH(QStringList, arguments);
        QFETCH(QByteArray, token);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("MEDIABOXVPLAYER_CONTROL_TOKEN", QString::fromLatin1(token));
        QProcess process;
        process.setProcessEnvironment(environment);
        arguments.prepend(directory.filePath("uncreated"));
        arguments.prepend("--data-dir");
        arguments.prepend("--managed");
        process.start(QStringLiteral(VPLAYER_EXECUTABLE), arguments);
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 2);
        QVERIFY(!QFile::exists(directory.filePath("uncreated")));
        const auto error = process.readAllStandardError();
        QVERIFY(error.contains("Managed mode requires"));
        if (!token.isEmpty())
            QVERIFY(!error.contains(token));
    }

    void managedRequiresExplicitDirectory()
    {
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("MEDIABOXVPLAYER_CONTROL_TOKEN", QString(64, 'a'));
        QProcess process;
        process.setProcessEnvironment(environment);
        process.start(QStringLiteral(VPLAYER_EXECUTABLE), {"--managed"});
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 2);
        QVERIFY(process.readAllStandardError().contains("explicit --data-dir"));
    }

    void rejectsInvalidPort()
    {
        QProcess process;
        process.start(QStringLiteral(VPLAYER_EXECUTABLE), {"--port", "65536"});
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 2);
    }

    void isolatedWindowsSurviveRestart_data()
    {
        QTest::addColumn<bool>("managed");
        QTest::addColumn<bool>("existingTokenFile");
        QTest::newRow("standalone") << false << false;
        QTest::newRow("managed") << true << false;
        QTest::newRow("managed-preserves-standalone-token") << true << true;
    }

    void isolatedWindowsSurviveRestart()
    {
        QFETCH(bool, managed);
        QFETCH(bool, existingTokenFile);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile tokenFile(directory.filePath("control.token"));
        const QByteArray managedToken(64, 'c');
        const QByteArray standaloneToken(64, 'd');
        if (existingTokenFile) {
            QVERIFY(tokenFile.open(QIODevice::WriteOnly));
            QCOMPARE(tokenFile.write(standaloneToken + '\n'), 65);
            tokenFile.close();
        }
        VideoProcess player;
        QVERIFY(player.start(directory.path(), managed));
        QTRY_VERIFY_WITH_TIMEOUT(player.connectSocket(), 5000);
        QByteArray token = managedToken;
        if (!managed || existingTokenFile) {
            QVERIFY(tokenFile.open(QIODevice::ReadOnly));
            const auto fileToken = tokenFile.readAll().trimmed();
            if (managed) {
                QCOMPARE(fileToken, standaloneToken);
            } else {
                token = fileToken;
                QCOMPARE(token.size(), 64);
                QVERIFY(token != managedToken);
            }
            tokenFile.close();
        } else {
            QVERIFY(!tokenFile.exists());
        }

        auto reply = player.send({{"command", "status"}}, token);
        QVERIFY(reply.value("ok").toBool());
        QCOMPARE(reply.value("status").toObject().value("application").toString(), QString("MediaBoxVPlayer"));
        QVERIFY(reply.value("status").toObject().value("windows").toArray().isEmpty());
        const auto displays = reply.value("status").toObject().value("displays").toArray();
        QVERIFY(!displays.isEmpty());
        const auto screen = displays.first().toObject().value("id").toString();
        QVERIFY(!screen.isEmpty());

        if (managed && existingTokenFile) {
            reply = player.send({{"command", "status"}}, standaloneToken);
            QVERIFY(!reply.value("ok").toBool());
            QCOMPARE(reply.value("error").toObject().value("code").toString(), QString("unauthorized"));
        }

        const QJsonObject first{{"command", "configureWindow"}, {"windowId", "left"},
                                {"name", "Левый экран"}, {"screen", screen}, {"fullscreen", false}};
        reply = player.send(first, QByteArray(64, 'z'));
        QVERIFY(!reply.value("ok").toBool());
        QCOMPARE(reply.value("error").toObject().value("code").toString(), QString("unauthorized"));
        reply = player.send({{"command", "status"}}, token);
        QVERIFY(reply.value("status").toObject().value("windows").toArray().isEmpty());
        reply = player.send(first, token);
        QVERIFY(reply.value("ok").toBool());
        QCOMPARE(window(reply, "left").value("actualScreen").toString(), screen);
        auto second = first;
        second["windowId"] = "right";
        second["name"] = "Правый экран";
        reply = player.send(second, token);
        QVERIFY(reply.value("ok").toBool());
        QCOMPARE(reply.value("status").toObject().value("windows").toArray().size(), 2);

        // Valid PCM fixtures keep this process test independent of optional
        // video codecs; real video frame delivery is checked separately.
        const QString leftPath = directory.filePath("Левый.wav");
        const QString rightPath = directory.filePath("Правый.wav");
        for (const auto &path : {leftPath, rightPath}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QDataStream stream(&file);
            stream.setByteOrder(QDataStream::LittleEndian);
            stream.writeRawData("RIFF", 4);
            stream << quint32(16036);
            stream.writeRawData("WAVEfmt ", 8);
            stream << quint32(16) << quint16(1) << quint16(1) << quint32(8000)
                   << quint32(16000) << quint16(2) << quint16(16);
            stream.writeRawData("data", 4);
            stream << quint32(16000);
            QCOMPARE(file.write(QByteArray(16000, '\0')), qint64(16000));
        }
        for (const auto &entry : {qMakePair(QString("left"), leftPath), qMakePair(QString("right"), rightPath)}) {
            reply = player.send({{"command", "load"}, {"windowId", entry.first},
                                 {"paths", QJsonArray{entry.second}}, {"autoplay", false}}, token);
            QVERIFY(reply.value("ok").toBool());
        }
        QCOMPARE(window(reply, "left").value("playback").toObject().value("queue").toArray(), QJsonArray{leftPath});
        QCOMPARE(window(reply, "right").value("playback").toObject().value("queue").toArray(), QJsonArray{rightPath});
        for (int revision = 1; revision <= 2; ++revision) {
            const auto publication = writePublication(directory.path(), revision);
            QVERIFY(!publication.isEmpty());
            QVERIFY(QJsonDocument(publication).toJson().size() < 2048);
            reply = player.send(publication, token);
            QVERIFY2(reply.value("ok").toBool(), QJsonDocument(reply).toJson().constData());
            QCOMPARE(window(reply, "left").value("playback").toObject().value("revision").toInt(), revision);
        }
        reply = player.send({{"command", "fullscreen"}, {"windowId", "left"}, {"value", true}}, token);
        QVERIFY(reply.value("ok").toBool());
        QVERIFY(window(reply, "left").value("fullscreen").toBool());
        QVERIFY(!window(reply, "right").value("fullscreen").toBool());

        auto invalid = first;
        invalid["screen"] = "missing-monitor";
        reply = player.send(invalid, token);
        QVERIFY(!reply.value("ok").toBool());
        QCOMPARE(window(reply, "left").value("screen").toString(), screen);
        QVERIFY(window(reply, "left").value("fullscreen").toBool());
        reply = player.send({{"command", "clear"}, {"windowId", "unknown-window"}}, token);
        QVERIFY(!reply.value("ok").toBool());
        QCOMPARE(window(reply, "right").value("playback").toObject().value("queue").toArray(), QJsonArray{rightPath});
        QCOMPARE(window(reply, "left").value("playback").toObject().value("revision").toInt(), 2);
        QVERIFY(window(reply, "left").value("playback").toObject().value("scheduleAvailable").toBool());

        QProcess duplicate;
        duplicate.start(QStringLiteral(VPLAYER_EXECUTABLE),
                        {"--data-dir", directory.path(), "--port", player.port == 17656 ? "17657" : "17656"});
        QVERIFY(duplicate.waitForFinished(5000));
        QCOMPARE(duplicate.exitCode(), 1);

        const bool gracefullyStopped = player.stop();
#ifdef Q_OS_UNIX
        QVERIFY(gracefullyStopped);
        QCOMPARE(player.process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(player.process.exitCode(), 0);
        QVERIFY(!QFile::exists(directory.filePath("player.lock")));
#else
        Q_UNUSED(gracefullyStopped);
#endif
        QVERIFY(!player.process.readAllStandardError().contains(token));
        QVERIFY(player.start(directory.path(), managed));
        QTRY_VERIFY_WITH_TIMEOUT(player.connectSocket(), 5000);
        reply = player.send({{"command", "status"}}, token);
        QVERIFY(reply.value("ok").toBool());
        QCOMPARE(reply.value("status").toObject().value("windows").toArray().size(), 2);
        QCOMPARE(window(reply, "left").value("playback").toObject().value("queue").toArray(), QJsonArray{leftPath});
        QCOMPARE(window(reply, "right").value("playback").toObject().value("queue").toArray(), QJsonArray{rightPath});
        QCOMPARE(window(reply, "left").value("playback").toObject().value("revision").toInt(), 2);
        QVERIFY(window(reply, "left").value("playback").toObject().value("scheduleAvailable").toBool());
        if (managed && existingTokenFile) {
            QVERIFY(tokenFile.open(QIODevice::ReadOnly));
            QCOMPARE(tokenFile.readAll(), standaloneToken + '\n');
            tokenFile.close();
        } else if (managed) {
            QVERIFY(!tokenFile.exists());
        }
        for (const auto &id : {QString("left"), QString("right")}) {
            const auto playback = window(reply, id).value("playback").toObject();
            QVERIFY(!playback.value("playbackRequested").toBool());
            QCOMPARE(playback.value("state").toString(), QString("stopped"));
        }
        for (const auto &id : {QString("left"), QString("right")}) {
            reply = player.send({{"command", "removeWindow"}, {"windowId", id}}, token);
            QVERIFY(reply.value("ok").toBool());
        }
        reply = player.send({{"command", "status"}}, token);
        QVERIFY(reply.value("ok").toBool());
        QVERIFY(reply.value("status").toObject().value("windows").toArray().isEmpty());
        QCOMPARE(player.process.state(), QProcess::Running);
    }
};

QTEST_GUILESS_MAIN(VideoProcessTests)
#include "tst_vplayerprocess.moc"
