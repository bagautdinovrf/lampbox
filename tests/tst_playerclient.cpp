#include "mediaboxplayerclient.h"
#include "audiobackend.h"
#include "controlserver.h"
#include "playerengine.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <functional>
#include <memory>

namespace {
using Client = MediaBoxPlayerClient;
using State = Client::ConnectionState;

QString testToken()
{
    return QString(64, QLatin1Char('a'));
}

PlayerConnectionSettings settingsFor(quint16 port)
{
    return {QStringLiteral("127.0.0.1"), port, testToken()};
}

Client::Timing testTiming(int responseMs = 1000, int reconnectMs = 40, int pollMs = 10000)
{
    return {pollMs, 500, responseMs, reconnectMs, reconnectMs};
}

QJsonObject snapshot(const QStringList &queue = {}, int index = 0,
                     const QString &state = QStringLiteral("stopped"))
{
    const int currentIndex = queue.isEmpty() ? -1 : index;
    return {{"state", state}, {"playbackRequested", state == "playing" || state == "loading"},
            {"queue", QJsonArray::fromStringList(queue)}, {"currentIndex", currentIndex},
            {"currentTrack", currentIndex < 0 ? QString() : queue.at(currentIndex)},
            {"positionMs", 0}, {"durationMs", 0}, {"volumePercent", 100},
            {"muted", false}, {"repeat", "off"}, {"error", ""}};
}

QByteArray frame(const QJsonObject &value)
{
    return QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n';
}

// This peer records requests for assertions but never prints authenticated frames.
class ControlledPeer final : public QObject
{
public:
    struct Request {
        QPointer<QTcpSocket> socket;
        QJsonObject object;
        QByteArray bytes;
    };

    ControlledPeer()
    {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (server.hasPendingConnections()) {
                QTcpSocket *socket = server.nextPendingConnection();
                ++connectionCount;
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    while (socket->canReadLine()) {
                        const QByteArray bytes = socket->readLine();
                        requests.append({socket, QJsonDocument::fromJson(bytes).object(), bytes});
                    }
                });
            }
        });
        listening = server.listen(QHostAddress::LocalHost, 0);
    }

    quint16 port() const { return server.serverPort(); }

    QJsonObject reply(int index, const QJsonObject &status = snapshot()) const
    {
        return {{"protocolVersion", 1}, {"id", requests.at(index).object.value("id")},
                {"ok", true}, {"status", status}};
    }

    void write(int index, const QByteArray &bytes)
    {
        if (requests.at(index).socket
            && requests.at(index).socket->state() == QAbstractSocket::ConnectedState) {
            requests.at(index).socket->write(bytes);
            requests.at(index).socket->flush();
        }
    }

    void answer(int index, const QJsonObject &status = snapshot())
    {
        write(index, frame(reply(index, status)));
    }

    void abort(int index)
    {
        if (requests.at(index).socket)
            requests.at(index).socket->abort();
    }

    QTcpServer server;
    QList<Request> requests;
    int connectionCount = 0;
    bool listening = false;
};

class TestBackend final : public MediaBox::AudioBackend
{
public:
    void setSource(const QUrl &) override { ++sourceCalls; }
    void play() override
    {
        ++playCalls;
        if (confirmPlayback)
            emit stateChanged(State::Playing);
    }
    void pause() override { emit stateChanged(State::Paused); }
    void stop() override { emit stateChanged(State::Stopped); }
    void seek(qint64 positionMs) override { emit positionChanged(positionMs); }
    void setVolume(int) override {}
    void setMuted(bool) override {}
    void confirmPlaying() { emit stateChanged(State::Playing); }
    bool confirmPlayback = true;
    int sourceCalls = 0;
    int playCalls = 0;
};

QString createTrack(const QTemporaryDir &directory, const QString &name)
{
    const QString path = directory.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write("test audio") != 10)
        return {};
    return path;
}

bool containsResult(const QSignalSpy &spy, const QString &id)
{
    for (const QList<QVariant> &arguments : spy) {
        if (arguments.at(0).toString() == id)
            return true;
    }
    return false;
}
} // namespace

class PlayerClientTests final : public QObject
{
    Q_OBJECT
private slots:
    void handshakeAndAllCommandEnvelopes()
    {
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming());
        QSignalSpy succeeded(&client, &Client::commandSucceeded);
        QSignalSpy failed(&client, &Client::commandFailed);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        QCOMPARE(client.connectionState(), State::Synchronizing);
        QVERIFY(!client.isReady());
        QVERIFY(client.play().isEmpty());
        QCOMPARE(failed.size(), 1);
        peer.answer(0);
        QTRY_VERIFY(client.isReady());

        const QStringList paths{QStringLiteral("/srv/Музыка/Первый.wav"),
                                QStringLiteral("C:/Media/Второй.wav")};
        const QJsonObject schedule{
            {"schemaVersion", 1},
            {"channels", QJsonArray{QJsonObject{{"name", "Утро"},
                                                {"paths", QJsonArray::fromStringList(paths)}}}},
            {"adverts", QJsonArray{}}
        };
        struct Command {
            QString name;
            QJsonObject fields;
            std::function<QString()> send;
        };
        const QList<Command> commands{
            {"status", {}, [&] { return client.requestStatus(); }},
            {"load", {{"paths", QJsonArray::fromStringList(paths)}, {"startIndex", 1}, {"autoplay", true}},
             [&] { return client.load(paths, 1, true); }},
            {"enqueue", {{"paths", QJsonArray::fromStringList(paths)}}, [&] { return client.enqueue(paths); }},
            {"play", {}, [&] { return client.play(); }},
            {"pause", {}, [&] { return client.pause(); }},
            {"stop", {}, [&] { return client.stop(); }},
            {"next", {}, [&] { return client.next(); }},
            {"previous", {}, [&] { return client.previous(); }},
            {"seek", {{"positionMs", 1234567890123.0}}, [&] { return client.seek(1234567890123LL); }},
            {"volume", {{"value", 37}}, [&] { return client.setVolume(37); }},
            {"mute", {{"value", true}}, [&] { return client.setMuted(true); }},
            {"repeat", {{"mode", "all"}}, [&] { return client.setRepeat("all"); }},
            {"clear", {}, [&] { return client.clear(); }},
            {"setSchedule", {{"schedule", schedule}}, [&] { return client.setSchedule(schedule); }},
            {"schedule", {}, [&] { return client.startSchedule(); }},
            {"schedule", {{"schedule", schedule}}, [&] { return client.startSchedule(schedule); }},
            {"playChannel", {{"name", "Утро"}, {"paths", QJsonArray::fromStringList(paths)}, {"volume", 63}, {"order", "shuffle_cycle"}},
             [&] { return client.playChannel(QStringLiteral("Утро"), paths, 63); }},
            {"playChannel", {{"name", "Утро"}, {"paths", QJsonArray::fromStringList(paths)}, {"volume", 63}, {"order", "sequential"}},
             [&] { return client.playChannel(QStringLiteral("Утро"), paths, 63, QStringLiteral("sequential")); }}
        };
        QSet<QString> ids{peer.requests.at(0).object.value("id").toString()};
        QVERIFY(!ids.contains(QString()));
        for (const Command &command : commands) {
            const int index = peer.requests.size();
            const int previousVolume = client.status().volumePercent;
            const QString id = command.send();
            QVERIFY(!id.isEmpty());
            QVERIFY(!ids.contains(id));
            ids.insert(id);
            QTRY_COMPARE(peer.requests.size(), index + 1);
            const auto &request = peer.requests.at(index);
            QVERIFY(request.object.value("protocolVersion").isDouble());
            QCOMPARE(request.object.value("protocolVersion").toInt(), 1);
            QVERIFY(request.object.value("id").isString());
            QCOMPARE(request.object.value("id").toString(), id);
            QVERIFY(request.object.value("token").isString());
            QVERIFY(request.object.value("token").toString() == testToken());
            QVERIFY(request.bytes.endsWith('\n'));
            QCOMPARE(request.bytes.count('\n'), 1);
            QVERIFY(request.bytes.size() <= 1024 * 1024);
            QJsonObject body = request.object;
            body.remove("token");
            body.remove("id");
            body.remove("protocolVersion");
            QJsonObject expected = command.fields;
            expected.insert("command", command.name);
            QCOMPARE(body, expected);
            QCOMPARE(client.status().volumePercent, previousVolume);
            QJsonObject confirmed = snapshot();
            confirmed.insert("volumePercent", index);
            peer.answer(index, confirmed);
            QTRY_VERIFY(containsResult(succeeded, id));
            QCOMPARE(client.status().volumePercent, index);
        }
        QCOMPARE(failed.size(), 1);
    }

    void realServerPlaybackErrorsAndReconnect()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, QStringLiteral("Первый.wav"));
        const QString second = createTrack(directory, QStringLiteral("Второй.wav"));
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        TestBackend backend;
        MediaBox::PlayerEngine engine(&backend);
        MediaBox::ControlServer server(&engine, testToken().toLatin1());
        QString error;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        Client client;
        client.setTiming(testTiming());
        QSignalSpy succeeded(&client, &Client::commandSucceeded);
        QSignalSpy failed(&client, &Client::commandFailed);
        client.connectToPlayer(settingsFor(server.port()));
        QTRY_VERIFY(client.isReady());
        QCOMPARE(client.status().currentIndex, -1);
        const QString emptyPlay = client.play();
        QTRY_VERIFY(containsResult(failed, emptyPlay));
        QCOMPARE(failed.last().at(2).toString(), QStringLiteral("empty_queue"));
        QVERIFY(client.isReady());
        QVERIFY(client.status().queue.isEmpty());

        QString id = client.load({first, second});
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().queue, QStringList({first, second}));
        QCOMPARE(client.status().state, QStringLiteral("stopped"));
        id = client.enqueue({first});
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().queue.size(), 3);
        id = client.setVolume(37);
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().volumePercent, 37);
        id = client.setMuted(true);
        QTRY_VERIFY(containsResult(succeeded, id));
        QVERIFY(client.status().muted);
        id = client.setRepeat("all");
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().repeat, QStringLiteral("all"));

        backend.confirmPlayback = false;
        id = client.play();
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().state, QStringLiteral("loading"));
        QVERIFY(client.status().playbackRequested);
        backend.confirmPlaying();
        id = client.requestStatus();
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().state, QStringLiteral("playing"));
        id = client.seek(1200);
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().positionMs, 1200);
        id = client.pause();
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().state, QStringLiteral("paused"));
        id = client.next();
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().currentIndex, 1);
        id = client.previous();
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().currentIndex, 0);
        backend.confirmPlayback = true;
        id = client.play();
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().state, QStringLiteral("playing"));

        const int sourceCalls = backend.sourceCalls;
        const int playCalls = backend.playCalls;
        client.disconnectFromPlayer();
        QCOMPARE(client.connectionState(), State::Disconnected);
        QCOMPARE(engine.status().value("state").toString(), QStringLiteral("playing"));
        client.connectToPlayer(settingsFor(server.port()));
        QTRY_VERIFY(client.isReady());
        QCOMPARE(client.status().queue.size(), 3);
        QCOMPARE(client.status().state, QStringLiteral("playing"));
        QCOMPARE(client.status().volumePercent, 37);
        QVERIFY(client.status().muted);
        QCOMPARE(backend.sourceCalls, sourceCalls);
        QCOMPARE(backend.playCalls, playCalls);

        id = client.load({directory.filePath("missing.wav")});
        QTRY_VERIFY(containsResult(failed, id));
        QCOMPARE(failed.last().at(2).toString(), QStringLiteral("invalid_path"));
        QCOMPARE(client.status().queue.size(), 3);
        QCOMPARE(client.status().state, QStringLiteral("playing"));
        id = client.stop();
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().state, QStringLiteral("stopped"));
        QCOMPARE(client.status().positionMs, 0);
        QCOMPARE(client.status().queue.size(), 3);
        id = client.play();
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().state, QStringLiteral("playing"));
        id = client.clear();
        QTRY_VERIFY(containsResult(succeeded, id));
        QVERIFY(client.status().queue.isEmpty());
        QCOMPARE(client.status().currentIndex, -1);
    }

    void fragmentedUtf8AndPartialTail()
    {
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming(1000, 500));
        QSignalSpy changed(&client, &Client::statusChanged);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        const QString path = QStringLiteral("/srv/Музыка/Песня\nдня.wav");
        const QByteArray bytes = frame(peer.reply(0, snapshot({path}, 0, "loading")));
        const int split = bytes.indexOf(QStringLiteral("П").toUtf8()) + 1;
        QVERIFY(split > 0);
        peer.write(0, bytes.first(split));
        QTest::qWait(20);
        QVERIFY(!client.isReady());
        QCOMPARE(changed.size(), 0);
        peer.write(0, bytes.mid(split, 7));
        QTest::qWait(20);
        QCOMPARE(changed.size(), 0);
        peer.write(0, bytes.mid(split + 7) + "{\"broken\":");
        QTRY_VERIFY(client.isReady());
        QCOMPARE(client.status().queue, QStringList{path});
        QCOMPARE(client.status().state, QStringLiteral("loading"));
        QCOMPARE(changed.size(), 1);
        peer.write(0, "}\n");
        QTRY_COMPARE(client.connectionState(), State::Reconnecting);
        QCOMPARE(client.status().queue, QStringList{path});
    }

    void everyCoalescedLineIsChecked()
    {
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming(1000, 500));
        QSignalSpy changed(&client, &Client::statusChanged);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        QJsonObject unexpected = peer.reply(0);
        unexpected.insert("id", "unsolicited");
        peer.write(0, frame(peer.reply(0)) + frame(unexpected));
        QTRY_COMPARE(client.connectionState(), State::Reconnecting);
        QCOMPARE(changed.size(), 1);
        QVERIFY(!client.isReady());
    }

    void terminalHandshakeErrors_data()
    {
        QTest::addColumn<QString>("errorCode");
        QTest::addColumn<int>("version");
        QTest::addColumn<State>("expectedState");
        QTest::newRow("unauthorized") << QStringLiteral("unauthorized") << 1 << State::AuthenticationFailed;
        QTest::newRow("unsupported-protocol") << QStringLiteral("unsupported_protocol") << 1 << State::ProtocolMismatch;
        QTest::newRow("new-protocol-version") << QString() << 2 << State::ProtocolMismatch;
    }

    void terminalHandshakeErrors()
    {
        QFETCH(QString, errorCode);
        QFETCH(int, version);
        QFETCH(State, expectedState);
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming());
        QSignalSpy errors(&client, &Client::connectionError);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        QJsonObject reply = peer.reply(0);
        reply.insert("protocolVersion", version);
        if (!errorCode.isEmpty()) {
            reply.insert("ok", false);
            reply.remove("status");
            reply.insert("error", QJsonObject{{"code", errorCode}, {"message", "Connection rejected"}});
        }
        peer.write(0, frame(reply));
        QTRY_COMPARE(client.connectionState(), expectedState);
        QVERIFY(!client.isReady());
        QVERIFY(!errors.isEmpty());
        QVERIFY(!errors.last().at(0).toString().isEmpty());
        QTest::qWait(150);
        QCOMPARE(peer.connectionCount, 1);
        QCOMPARE(peer.requests.size(), 1);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 2);
        QCOMPARE(peer.requests.at(1).object.value("command").toString(), QStringLiteral("status"));
        peer.answer(1);
        QTRY_VERIFY(client.isReady());
    }

    void malformedResponses_data()
    {
        QTest::addColumn<QString>("kind");
        for (const char *kind : {"json", "array", "id", "missing-status", "missing-field", "enum", "type", "index", "track",
                                 "playback-mode-type", "playback-mode-enum", "playback-mode-null",
                                 "channel-name-type", "schedule-available-type", "schedule-error-type"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }

    void malformedResponses()
    {
        QFETCH(QString, kind);
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming(1000, 500));
        QSignalSpy changed(&client, &Client::statusChanged);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        QJsonObject reply = peer.reply(0);
        QJsonObject status = snapshot();
        if (kind == "json") {
            peer.write(0, "{broken}\n");
        } else if (kind == "array") {
            peer.write(0, "[]\n");
        } else {
            if (kind == "id")
                reply.insert("id", "wrong-request");
            else if (kind == "missing-status")
                reply.remove("status");
            else {
                if (kind == "missing-field") status.remove("muted");
                if (kind == "enum") status.insert("state", "buffering");
                if (kind == "type") status.insert("volumePercent", "70");
                if (kind == "index") status.insert("currentIndex", 0);
                if (kind == "track") status.insert("currentTrack", "/srv/ghost.wav");
                if (kind == "playback-mode-type") status.insert("playbackMode", false);
                if (kind == "playback-mode-enum") status.insert("playbackMode", "automatic");
                if (kind == "playback-mode-null") status.insert("playbackMode", QJsonValue::Null);
                if (kind == "channel-name-type") status.insert("channelName", 12);
                if (kind == "schedule-available-type") status.insert("scheduleAvailable", "true");
                if (kind == "schedule-error-type") status.insert("scheduleError", QJsonArray{});
                reply.insert("status", status);
            }
            peer.write(0, frame(reply));
        }
        QTRY_COMPARE(client.connectionState(), State::Reconnecting);
        QVERIFY(!client.isReady());
        QCOMPARE(changed.size(), 0);
    }

    void optionalScheduleStatusIsBackwardCompatible_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::newRow("manual-channel") << QStringLiteral("manual");
        QTest::newRow("schedule") << QStringLiteral("schedule");
    }

    void optionalScheduleStatusIsBackwardCompatible()
    {
        QFETCH(QString, mode);
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming());
        QSignalSpy succeeded(&client, &Client::commandSucceeded);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0);
        QTRY_VERIFY(client.isReady());
        QCOMPARE(client.status().playbackMode, QStringLiteral("manual"));
        QVERIFY(client.status().channelName.isEmpty());
        QVERIFY(!client.status().scheduleAvailable);
        QVERIFY(client.status().scheduleError.isEmpty());

        const QString id = client.requestStatus();
        QVERIFY(!id.isEmpty());
        QTRY_COMPARE(peer.requests.size(), 2);
        QJsonObject scheduled = snapshot({"/srv/Музыка/Утро.wav"}, 0, "playing");
        scheduled.insert("playbackMode", mode);
        scheduled.insert("channelName", "Утро");
        scheduled.insert("scheduleAvailable", true);
        scheduled.insert("scheduleError", "Не найден файл рекламы");
        peer.answer(1, scheduled);
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().playbackMode, mode);
        QCOMPARE(client.status().channelName, QStringLiteral("Утро"));
        QVERIFY(client.status().scheduleAvailable);
        QCOMPARE(client.status().scheduleError, QStringLiteral("Не найден файл рекламы"));

        const QString legacy = client.requestStatus();
        QVERIFY(!legacy.isEmpty());
        QTRY_COMPARE(peer.requests.size(), 3);
        peer.answer(2);
        QTRY_VERIFY(containsResult(succeeded, legacy));
        QCOMPARE(client.status().playbackMode, QStringLiteral("manual"));
        QVERIFY(client.status().channelName.isEmpty());
        QVERIFY(!client.status().scheduleAvailable);
        QVERIFY(client.status().scheduleError.isEmpty());
    }

    void statusLargerThanRequestLimit()
    {
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming(3000));
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        QStringList paths;
        for (int i = 0; i < 600; ++i)
            paths.append(QStringLiteral("/srv/") + QString(3000, QLatin1Char('x')) + QString::number(i) + ".wav");
        const QByteArray response = frame(peer.reply(0, snapshot(paths, 599)));
        QVERIFY(response.size() > 1024 * 1024);
        QVERIFY(response.size() < 32 * 1024 * 1024);
        peer.write(0, response);
        QTRY_VERIFY_WITH_TIMEOUT(client.isReady(), 3000);
        QCOMPARE(client.status().queue, paths);
        QCOMPARE(client.status().currentIndex, 599);
    }

    void oversizedResponse_data()
    {
        QTest::addColumn<bool>("terminated");
        QTest::newRow("partial-tail") << false;
        QTest::newRow("full-line") << true;
    }

    void oversizedResponse()
    {
        QFETCH(bool, terminated);
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming(5000, 1000));
        QSignalSpy changed(&client, &Client::statusChanged);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        QByteArray response(32 * 1024 * 1024 + 1, ' ');
        if (terminated)
            response.append('\n');
        peer.write(0, response);
        QTRY_COMPARE_WITH_TIMEOUT(client.connectionState(), State::Reconnecting, 5000);
        QCOMPARE(changed.size(), 0);
        QVERIFY(!client.isReady());
    }

    void lostMutationIsUnknownAndNeverReplayed_data()
    {
        QTest::addColumn<QString>("command");
        QTest::newRow("enqueue") << QStringLiteral("enqueue");
        QTest::newRow("next") << QStringLiteral("next");
    }

    void lostMutationIsUnknownAndNeverReplayed()
    {
        QFETCH(QString, command);
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming());
        QSignalSpy unknown(&client, &Client::commandOutcomeUnknown);
        QSignalSpy cancelled(&client, &Client::commandCancelled);
        QSignalSpy succeeded(&client, &Client::commandSucceeded);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        const QStringList before{"/srv/first.wav", "/srv/second.wav"};
        peer.answer(0, snapshot(before, 0, "playing"));
        QTRY_VERIFY(client.isReady());
        const QString lost = command == "enqueue" ? client.enqueue({"/srv/third.wav"}) : client.next();
        const QString queued = client.pause();
        QVERIFY(!lost.isEmpty());
        QVERIFY(!queued.isEmpty());
        QTRY_COMPARE(peer.requests.size(), 2);
        QCOMPARE(peer.requests.at(1).object.value("command").toString(), command);
        peer.abort(1);
        QTRY_VERIFY(containsResult(unknown, lost));
        QTRY_VERIFY(containsResult(cancelled, queued));
        QCOMPARE(unknown.size(), 1);
        QCOMPARE(cancelled.size(), 1);
        QVERIFY(!containsResult(succeeded, lost));
        QCOMPARE(client.status().state, QStringLiteral("playing"));
        QTRY_COMPARE(peer.requests.size(), 3);
        QCOMPARE(peer.requests.at(2).object.value("command").toString(), QStringLiteral("status"));
        QVERIFY(peer.requests.at(2).object.value("id") != peer.requests.at(0).object.value("id"));
        QStringList actual = before;
        if (command == "enqueue") actual.append("/srv/third.wav");
        peer.answer(2, snapshot(actual, command == "next" ? 1 : 0, "playing"));
        QTRY_VERIFY(client.isReady());
        QCOMPARE(client.status().queue, actual);
        QCOMPARE(client.status().currentIndex, command == "next" ? 1 : 0);
        QTest::qWait(100);
        QCOMPARE(peer.requests.size(), 3);
        QVERIFY(!containsResult(succeeded, lost));
    }

    void responseTimeoutKeepsEventLoopResponsive()
    {
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming(100, 500));
        QSignalSpy unknown(&client, &Client::commandOutcomeUnknown);
        QSignalSpy cancelled(&client, &Client::commandCancelled);
        int ticks = 0;
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, this, [&] { ++ticks; });
        heartbeat.start(5);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0);
        QTRY_VERIFY(client.isReady());
        const QString lost = client.setVolume(10);
        const QString queued = client.play();
        QTRY_COMPARE(peer.requests.size(), 2);
        QTRY_COMPARE(client.connectionState(), State::Reconnecting);
        QVERIFY(containsResult(unknown, lost));
        QVERIFY(containsResult(cancelled, queued));
        QVERIFY(ticks >= 5);
        QCOMPARE(client.status().volumePercent, 100);
        client.disconnectFromPlayer();
        QTest::qWait(550);
        QCOMPARE(client.connectionState(), State::Disconnected);
        QCOMPARE(peer.connectionCount, 1);
    }

    void settingsSwitchDiscardsOldReplyAndTimers()
    {
        ControlledPeer oldPeer;
        ControlledPeer newPeer;
        QVERIFY(oldPeer.listening);
        QVERIFY(newPeer.listening);
        Client client;
        client.setTiming(testTiming(150, 30));
        QSignalSpy unknown(&client, &Client::commandOutcomeUnknown);
        QSignalSpy cancelled(&client, &Client::commandCancelled);
        client.connectToPlayer(settingsFor(oldPeer.port()));
        QTRY_COMPARE(oldPeer.requests.size(), 1);
        oldPeer.answer(0);
        QTRY_VERIFY(client.isReady());
        const QString sent = client.enqueue({"/srv/old.wav"});
        const QString queued = client.play();
        QTRY_COMPARE(oldPeer.requests.size(), 2);
        const QByteArray oldReply = frame(oldPeer.reply(1, snapshot({"/srv/old.wav"}, 0, "playing")));
        oldPeer.write(1, oldReply.first(oldReply.size() / 2));
        QTest::qWait(10);
        client.connectToPlayer(settingsFor(newPeer.port()));
        QVERIFY(containsResult(unknown, sent));
        QVERIFY(containsResult(cancelled, queued));
        QTRY_COMPARE(newPeer.requests.size(), 1);
        QCOMPARE(newPeer.requests.at(0).object.value("command").toString(), QStringLiteral("status"));
        QJsonObject newStatus = snapshot({"/srv/new.wav"}, 0, "paused");
        newStatus.insert("volumePercent", 23);
        newPeer.answer(0, newStatus);
        QTRY_VERIFY(client.isReady());
        // A queued write on the obsolete socket cannot change the new session.
        oldPeer.write(1, oldReply.mid(oldReply.size() / 2));
        QTest::qWait(200);
        QVERIFY(client.isReady());
        QCOMPARE(client.status().queue, QStringList{"/srv/new.wav"});
        QCOMPARE(client.status().volumePercent, 23);
        QCOMPARE(oldPeer.connectionCount, 1);
        QCOMPARE(newPeer.connectionCount, 1);
        QCOMPARE(newPeer.requests.size(), 1);
        client.disconnectFromPlayer();
        QTest::qWait(80);
        QCOMPARE(client.connectionState(), State::Disconnected);
        QCOMPARE(newPeer.connectionCount, 1);
    }

    void restartedServerFreshStateReplacesSnapshot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = createTrack(directory, "track.wav");
        QVERIFY(!path.isEmpty());
        TestBackend backend;
        auto engine = std::make_unique<MediaBox::PlayerEngine>(&backend);
        auto server = std::make_unique<MediaBox::ControlServer>(engine.get(), testToken().toLatin1());
        QString error;
        QVERIFY2(server->listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        const quint16 port = server->port();
        Client client;
        client.setTiming(testTiming(1000, 150));
        QSignalSpy succeeded(&client, &Client::commandSucceeded);
        client.connectToPlayer(settingsFor(port));
        QTRY_VERIFY(client.isReady());
        QString id = client.load({path}, 0, true);
        QTRY_VERIFY(containsResult(succeeded, id));
        id = client.setVolume(12);
        QTRY_VERIFY(containsResult(succeeded, id));
        id = client.setMuted(true);
        QTRY_VERIFY(containsResult(succeeded, id));
        id = client.setRepeat("all");
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().state, QStringLiteral("playing"));
        const int sourcesBeforeRestart = backend.sourceCalls;
        server.reset();
        engine.reset();
        QTRY_COMPARE(client.connectionState(), State::Reconnecting);
        engine = std::make_unique<MediaBox::PlayerEngine>(&backend);
        server = std::make_unique<MediaBox::ControlServer>(engine.get(), testToken().toLatin1());
        QVERIFY2(server->listen(QHostAddress::LocalHost, port, &error), qPrintable(error));
        QTRY_VERIFY(client.isReady());
        QVERIFY(client.status().queue.isEmpty());
        QCOMPARE(client.status().currentIndex, -1);
        QCOMPARE(client.status().state, QStringLiteral("stopped"));
        QCOMPARE(client.status().volumePercent, 100);
        QVERIFY(!client.status().muted);
        QCOMPARE(client.status().repeat, QStringLiteral("off"));
        QCOMPARE(backend.sourceCalls, sourcesBeforeRestart);
    }

    void invalidAndOversizedRequestsStayLocal()
    {
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming());
        QSignalSpy failed(&client, &Client::commandFailed);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0);
        QTRY_VERIFY(client.isReady());
        QVERIFY(client.setVolume(101).isEmpty());
        QVERIFY(client.seek(-1).isEmpty());
        QVERIFY(client.setRepeat("random").isEmpty());
        QVERIFY(client.load({}).isEmpty());
        QVERIFY(client.load({"relative.wav"}).isEmpty());
        QVERIFY(client.load({"/srv/track.wav"}, 1).isEmpty());
        QStringList paths;
        const QString path = "/srv/" + QString(3000, QLatin1Char('x')) + ".wav";
        for (int i = 0; i < 1000; ++i) paths.append(path);
        QVERIFY(client.load(paths).isEmpty());
        QVERIFY(client.setSchedule({}).isEmpty());
        QVERIFY(client.setSchedule({{"channels", QJsonArray{}}, {"adverts", "invalid"}}).isEmpty());
        QVERIFY(client.playChannel("   ", {"/srv/track.wav"}, 100).isEmpty());
        QVERIFY(client.playChannel("Утро", {}, 100).isEmpty());
        QVERIFY(client.playChannel("Утро", {"relative.wav"}, 100).isEmpty());
        QVERIFY(client.playChannel("Утро", {"/srv/track.wav"}, 101).isEmpty());
        QVERIFY(client.playChannel("Утро", {"/srv/track.wav"}, -1).isEmpty());
        QVERIFY(client.playChannel("Утро", {"/srv/track.wav"}, 100, "random").isEmpty());
        QCOMPARE(failed.size(), 15);
        for (const QList<QVariant> &result : failed) {
            QVERIFY(result.at(0).toString().isEmpty());
            QVERIFY(!result.at(2).toString().isEmpty());
            QVERIFY(!result.at(3).toString().contains(testToken()));
        }
        QTest::qWait(30);
        QCOMPARE(peer.requests.size(), 1);
        QVERIFY(client.isReady());
    }

    void commandFailureAndAudioErrorAreIndependent()
    {
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming());
        QSignalSpy failed(&client, &Client::commandFailed);
        QSignalSpy succeeded(&client, &Client::commandSucceeded);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0);
        QTRY_VERIFY(client.isReady());
        const QString rejected = client.play();
        QTRY_COMPARE(peer.requests.size(), 2);
        QJsonObject status = snapshot();
        status.insert("volumePercent", 31);
        QJsonObject response = peer.reply(1, status);
        response.insert("ok", false);
        response.insert("error", QJsonObject{{"code", "invalid_arguments"}, {"message", "Unexpected field"}});
        peer.write(1, frame(response));
        QTRY_VERIFY(containsResult(failed, rejected));
        QCOMPARE(failed.last().at(2).toString(), QStringLiteral("invalid_arguments"));
        QCOMPARE(client.status().volumePercent, 31);
        QVERIFY(client.status().error.isEmpty());
        QVERIFY(client.isReady());

        const QString accepted = client.play();
        QTRY_COMPARE(peer.requests.size(), 3);
        status = snapshot({"/srv/track.wav"}, 0, "error");
        status.insert("error", "Output unavailable");
        response = peer.reply(2, status);
        response.insert("futureExtension", QJsonObject{{"ignored", true}});
        peer.write(2, frame(response));
        QTRY_VERIFY(containsResult(succeeded, accepted));
        QCOMPARE(client.status().state, QStringLiteral("error"));
        QCOMPARE(client.status().error, QStringLiteral("Output unavailable"));
        QCOMPARE(failed.size(), 1);
        QVERIFY(client.isReady());
    }

#ifdef PLAYER_EXECUTABLE
    void realPlayerProcessRestart()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile tokenFile(directory.filePath("control.token"));
        QVERIFY(tokenFile.open(QIODevice::WriteOnly));
        QCOMPARE(tokenFile.write(testToken().toLatin1() + '\n'), 65);
        tokenFile.close();
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const quint16 port = reservation.serverPort();
        reservation.close();
        QProcess process;
        const QStringList arguments{"--data-dir", directory.path(), "--port", QString::number(port)};
        process.start(QStringLiteral(PLAYER_EXECUTABLE), arguments);
        QTRY_COMPARE_WITH_TIMEOUT(process.state(), QProcess::Running, 5000);
        Client client;
        client.setTiming(testTiming(1000, 50));
        QSignalSpy succeeded(&client, &Client::commandSucceeded);
        QSignalSpy failed(&client, &Client::commandFailed);
        client.connectToPlayer(settingsFor(port));
        QTRY_VERIFY_WITH_TIMEOUT(client.isReady(), 5000);
        QVERIFY(client.status().queue.isEmpty());
        QCOMPARE(client.status().state, QStringLiteral("stopped"));
        QString id = client.setVolume(19);
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(client.status().volumePercent, 19);
        id = client.stop();
        QTRY_VERIFY(containsResult(succeeded, id));
        QCOMPARE(process.state(), QProcess::Running);
        id = client.play();
        QTRY_VERIFY(containsResult(failed, id));
        QCOMPARE(failed.last().at(2).toString(), QStringLiteral("empty_queue"));
        QVERIFY(client.isReady());
        QCOMPARE(process.state(), QProcess::Running);
        const auto stopProcess = [&] {
#ifdef Q_OS_UNIX
            process.terminate();
#else
            process.kill();
#endif
        };
        stopProcess();
        QTRY_COMPARE_WITH_TIMEOUT(process.state(), QProcess::NotRunning, 5000);
        QTRY_VERIFY(!client.isReady());
        QVERIFY(!process.readAllStandardError().contains(testToken().toLatin1()));
        process.start(QStringLiteral(PLAYER_EXECUTABLE), arguments);
        QTRY_COMPARE_WITH_TIMEOUT(process.state(), QProcess::Running, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(client.isReady(), 5000);
        QVERIFY(client.status().queue.isEmpty());
        QCOMPARE(client.status().volumePercent, 100);
        QCOMPARE(client.status().state, QStringLiteral("stopped"));
        client.disconnectFromPlayer();
        stopProcess();
        QTRY_COMPARE_WITH_TIMEOUT(process.state(), QProcess::NotRunning, 5000);
        QVERIFY(!process.readAllStandardError().contains(testToken().toLatin1()));
    }
#endif

    void pollingDoesNotPipelineOrDuplicateStatus()
    {
        ControlledPeer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming(testTiming(1000, 40, 40));
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0);
        QTRY_VERIFY(client.isReady());
        QTRY_COMPARE(peer.requests.size(), 2);
        QCOMPARE(peer.requests.at(1).object.value("command").toString(), QStringLiteral("status"));
        const QString pendingStatus = peer.requests.at(1).object.value("id").toString();
        QCOMPARE(client.requestStatus(), pendingStatus);
        const QString volume = client.setVolume(42);
        QVERIFY(!volume.isEmpty());
        QTest::qWait(120);
        QCOMPARE(peer.requests.size(), 2);
        peer.answer(1);
        QTRY_VERIFY(peer.requests.size() >= 3);
        QCOMPARE(peer.requests.at(2).object.value("command").toString(), QStringLiteral("volume"));
        QCOMPARE(peer.requests.at(2).object.value("id").toString(), volume);
        peer.answer(2);
        QTRY_VERIFY(peer.requests.size() >= 4);
        QCOMPARE(peer.requests.at(3).object.value("command").toString(), QStringLiteral("status"));
        client.disconnectFromPlayer();
    }
};

QTEST_GUILESS_MAIN(PlayerClientTests)
#include "tst_playerclient.moc"
