#include <QFile>
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
class VideoProcess {
public:
    ~VideoProcess() { stop(); }

    bool start(const QString &directory)
    {
        QTcpServer reservation;
        if (!reservation.listen(QHostAddress::LocalHost, 0))
            return false;
        port = reservation.serverPort();
        reservation.close();
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        process.setProcessEnvironment(environment);
        process.start(QStringLiteral(VPLAYER_EXECUTABLE),
                      {"--data-dir", directory, "--port", QString::number(port)});
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
        process.terminate();
        const bool graceful = process.waitForFinished(5000);
        if (!graceful) {
            process.kill();
            process.waitForFinished(5000);
        }
        return graceful;
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
    void rejectsInvalidPort()
    {
        QProcess process;
        process.start(QStringLiteral(VPLAYER_EXECUTABLE), {"--port", "65536"});
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 2);
    }

    void isolatedWindowsSurviveRestart()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        VideoProcess player;
        QVERIFY(player.start(directory.path()));
        QTRY_VERIFY_WITH_TIMEOUT(player.connectSocket(), 5000);
        QFile tokenFile(directory.filePath("control.token"));
        QVERIFY(tokenFile.open(QIODevice::ReadOnly));
        const auto token = tokenFile.readAll().trimmed();
        QCOMPARE(token.size(), 64);
        tokenFile.close();

        auto reply = player.send({{"command", "status"}}, token);
        QVERIFY(reply.value("ok").toBool());
        QCOMPARE(reply.value("status").toObject().value("application").toString(), QString("MediaBoxVPlayer"));
        const auto displays = reply.value("status").toObject().value("displays").toArray();
        QVERIFY(!displays.isEmpty());
        const auto screen = displays.first().toObject().value("id").toString();
        QVERIFY(!screen.isEmpty());

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
        QVERIFY(player.start(directory.path()));
        QTRY_VERIFY_WITH_TIMEOUT(player.connectSocket(), 5000);
        reply = player.send({{"command", "status"}}, token);
        QVERIFY(reply.value("ok").toBool());
        QCOMPARE(reply.value("status").toObject().value("windows").toArray().size(), 2);
        QCOMPARE(window(reply, "left").value("playback").toObject().value("queue").toArray(), QJsonArray{leftPath});
        QCOMPARE(window(reply, "right").value("playback").toObject().value("queue").toArray(), QJsonArray{rightPath});
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
