#include "audiobackend.h"
#include "controlserver.h"
#include "playerengine.h"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

using namespace MediaBox;

class SilentBackend final : public AudioBackend
{
public:
    void setSource(const QUrl &) override {}
    void play() override {}
    void pause() override {}
    void stop() override {}
    void seek(qint64) override {}
    void setVolume(int) override {}
    void setMuted(bool) override {}
};

class ConfirmingBackend final : public AudioBackend
{
public:
    void setSource(const QUrl &) override {}
    void play() override { emit stateChanged(State::Playing); }
    void pause() override { emit stateChanged(State::Paused); }
    void stop() override { emit stateChanged(State::Stopped); }
    void seek(qint64 value) override { emit positionChanged(value); }
    void setVolume(int) override {}
    void setMuted(bool) override {}
};

class ControlTests final : public QObject
{
    Q_OBJECT
private slots:
    void allPlaybackControlsUseTcpAndSurviveReconnect()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString firstPath = directory.filePath(QStringLiteral("Первый трек.wav"));
        const QString secondPath = directory.filePath(QStringLiteral("Второй трек.wav"));
        for (const QString &path : {firstPath, secondPath}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }

        ConfirmingBackend backend;
        PlayerEngine engine(&backend);
        const QByteArray token(64, 'b');
        ControlServer server(&engine, token);
        QString error;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);

        int requestId = 0;
        const auto exchange = [&](QJsonObject request) {
            request.insert("id", ++requestId);
            request.insert("protocolVersion", 1);
            request.insert("token", QString::fromLatin1(token));
            client.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
            QElapsedTimer timer;
            timer.start();
            while (!client.canReadLine() && timer.elapsed() < 2000)
                QTest::qWait(1);
            const QJsonObject reply = QJsonDocument::fromJson(client.readLine()).object();
            if (!reply.value("ok").toBool() || reply.value("id").toInt() != requestId)
                return QJsonObject();
            return reply.value("status").toObject();
        };

        auto status = exchange({{"command", "load"}, {"paths", QJsonArray{firstPath, secondPath}}});
        QCOMPARE(status.value("queue").toArray().size(), 2);
        QCOMPARE(status.value("state").toString(), QStringLiteral("stopped"));
        status = exchange({{"command", "enqueue"}, {"paths", QJsonArray{firstPath}}});
        QCOMPARE(status.value("queue").toArray().size(), 3);
        QCOMPARE(exchange({{"command", "volume"}, {"value", 37}}).value("volumePercent").toInt(), 37);
        QVERIFY(exchange({{"command", "mute"}, {"value", true}}).value("muted").toBool());
        QCOMPARE(exchange({{"command", "repeat"}, {"mode", "all"}}).value("repeat").toString(), QStringLiteral("all"));
        QCOMPARE(exchange({{"command", "play"}}).value("state").toString(), QStringLiteral("playing"));
        QCOMPARE(exchange({{"command", "seek"}, {"positionMs", 1200}}).value("positionMs").toInt(), 1200);
        QCOMPARE(exchange({{"command", "pause"}}).value("state").toString(), QStringLiteral("paused"));
        QCOMPARE(exchange({{"command", "next"}}).value("currentIndex").toInt(-1), 1);
        QCOMPARE(exchange({{"command", "previous"}}).value("currentIndex").toInt(-1), 0);
        QCOMPARE(exchange({{"command", "play"}}).value("state").toString(), QStringLiteral("playing"));

        // Losing Manager's connection must not stop playback or change state.
        client.disconnectFromHost();
        QTRY_COMPARE(client.state(), QAbstractSocket::UnconnectedState);
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        status = exchange({{"command", "status"}});
        QCOMPARE(status.value("state").toString(), QStringLiteral("playing"));
        QCOMPARE(status.value("queue").toArray().size(), 3);
        QCOMPARE(status.value("volumePercent").toInt(), 37);
        QVERIFY(status.value("muted").toBool());
        QCOMPARE(status.value("repeat").toString(), QStringLiteral("all"));

        status = exchange({{"command", "stop"}});
        QCOMPARE(status.value("state").toString(), QStringLiteral("stopped"));
        QCOMPARE(status.value("positionMs").toInt(-1), 0);
        QCOMPARE(status.value("queue").toArray().size(), 3);
        // Playback stop keeps the same TCP service available for more commands.
        QCOMPARE(exchange({{"command", "play"}}).value("state").toString(), QStringLiteral("playing"));
        status = exchange({{"command", "clear"}});
        QCOMPARE(status.value("state").toString(), QStringLiteral("stopped"));
        QVERIFY(status.value("queue").toArray().isEmpty());
        QCOMPARE(status.value("currentIndex").toInt(), -1);
    }

    void authenticatedCommandsAndFraming()
    {
        SilentBackend backend;
        PlayerEngine engine(&backend);
        const QByteArray token(64, 'a');
        ControlServer server(&engine, token);
        QString error;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, server.port());
        QTRY_COMPARE(socket.state(), QAbstractSocket::ConnectedState);

        const auto request = [&](QJsonObject value) {
            value.insert(QStringLiteral("protocolVersion"), 1);
            value.insert(QStringLiteral("token"), QString::fromLatin1(token));
            return QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n';
        };
        const QByteArray volume = request({{"id", "first"}, {"command", "volume"}, {"value", 37}});
        socket.write(volume.first(9));
        socket.flush();
        QTest::qWait(20);
        QCOMPARE(engine.status().value("volumePercent").toInt(), 100);
        QVERIFY(!socket.canReadLine());
        socket.write(volume.mid(9) + request({{"id", 2}, {"command", "status"}}));
        QTRY_VERIFY(socket.canReadLine());
        const auto first = QJsonDocument::fromJson(socket.readLine()).object();
        QVERIFY(first.value("ok").toBool());
        QCOMPARE(first.value("id").toString(), QStringLiteral("first"));
        QCOMPARE(first.value("protocolVersion").toInt(), 1);
        QCOMPARE(first.value("status").toObject().value("volumePercent").toInt(), 37);
        QTRY_VERIFY(socket.canReadLine());
        const auto second = QJsonDocument::fromJson(socket.readLine()).object();
        QCOMPARE(second.value("id").toInt(), 2);
        QCOMPARE(second.value("status").toObject(), engine.status());

        socket.write("{\"protocolVersion\":1,\"command\":\"volume\",\"value\":0,\"token\":\"wrong\"}\n");
        QTRY_VERIFY(socket.canReadLine());
        const auto denied = QJsonDocument::fromJson(socket.readLine()).object();
        QVERIFY(!denied.value("ok").toBool());
        QCOMPARE(denied.value("error").toObject().value("code").toString(), QStringLiteral("unauthorized"));
        QVERIFY(!denied.contains("status"));
        QCOMPARE(engine.status().value("volumePercent").toInt(), 37);

        socket.write("not json\n[]\n");
        for (int i = 0; i < 2; ++i) {
            QTRY_VERIFY(socket.canReadLine());
            const auto reply = QJsonDocument::fromJson(socket.readLine()).object();
            QCOMPARE(reply.value("error").toObject().value("code").toString(), QStringLiteral("invalid_json"));
        }
        auto wrongVersion = QJsonDocument::fromJson(request({{"command", "clear"}})).object();
        wrongVersion.insert("protocolVersion", 2);
        socket.write(QJsonDocument(wrongVersion).toJson(QJsonDocument::Compact) + '\n');
        QTRY_VERIFY(socket.canReadLine());
        QCOMPARE(QJsonDocument::fromJson(socket.readLine()).object().value("error").toObject()
                     .value("code").toString(), QStringLiteral("unsupported_protocol"));
    }

    void oversizedRequestDoesNotExecute()
    {
        SilentBackend backend;
        PlayerEngine engine(&backend);
        ControlServer server(&engine, QByteArray(64, 'a'));
        QString error;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0, &error));
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, server.port());
        QTRY_COMPARE(socket.state(), QAbstractSocket::ConnectedState);
        socket.write(QByteArray(1024 * 1024 + 1, ' '));
        QTRY_COMPARE(socket.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(engine.status().value("volumePercent").toInt(), 100);
    }

    void tokenIsPersistentAndInvalidTokenIsNotReplaced()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QByteArray first;
        QString error;
        QVERIFY2(loadControlToken(directory.path(), &first, &error), qPrintable(error));
        QCOMPARE(first.size(), 64);
        QByteArray second;
        QVERIFY(loadControlToken(directory.path(), &second, &error));
        QCOMPARE(second, first);
        QFile file(directory.filePath("control.token"));
#ifdef Q_OS_UNIX
        QVERIFY(!(file.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup
                                       | QFileDevice::ReadOther | QFileDevice::WriteOther)));
#endif
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("invalid-token\n"), 14);
        file.close();
        QVERIFY(!loadControlToken(directory.path(), &second, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("invalid-token\n"));
    }
};

QTEST_GUILESS_MAIN(ControlTests)
#include "tst_controlserver.moc"
