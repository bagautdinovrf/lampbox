#include "mediaboxvplayerclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

#include <functional>

namespace {

using Client = MediaBoxVPlayerClient;
using State = Client::ConnectionState;

PlayerConnectionSettings settingsFor(quint16 port)
{
    return {QStringLiteral("127.0.0.1"), port, QString(64, QLatin1Char('a'))};
}

QJsonObject playback(const QStringList &paths = {})
{
    return {{"state", "stopped"}, {"playbackRequested", false},
            {"queue", QJsonArray::fromStringList(paths)}, {"currentIndex", paths.isEmpty() ? -1 : 0},
            {"currentTrack", paths.value(0)}, {"positionMs", 0}, {"durationMs", 0},
            {"volumePercent", 100}, {"muted", false}, {"repeat", "off"}, {"error", ""}};
}

QJsonObject snapshot()
{
    return {{"application", "MediaBoxVPlayer"},
            {"displays", QJsonArray{QJsonObject{{"id", "screen-1"}, {"name", "Primary"}, {"index", 0}},
                                    QJsonObject{{"id", "screen-2"}, {"name", "Projector"}, {"index", 1}}}},
            {"windows", QJsonArray{
                QJsonObject{{"id", "hall"}, {"name", "Зал"}, {"screen", "screen-1"},
                            {"actualScreen", "screen-1"}, {"fullscreen", true},
                            {"playback", playback({"/video/hall.mp4"})}},
                QJsonObject{{"id", "foyer"}, {"name", "Фойе"}, {"screen", "screen-2"},
                            {"actualScreen", "screen-2"}, {"fullscreen", false},
                            {"playback", playback({"C:/Video/foyer.mp4"})}}}}};
}

class Peer final : public QObject
{
public:
    struct Request {
        QPointer<QTcpSocket> socket;
        QJsonObject object;
    };

    Peer()
    {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (server.hasPendingConnections()) {
                auto *socket = server.nextPendingConnection();
                ++connections;
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    while (socket->canReadLine())
                        requests.append({socket, QJsonDocument::fromJson(socket->readLine()).object()});
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

    void send(int index, const QJsonObject &object)
    {
        if (requests.at(index).socket)
            requests.at(index).socket->write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
    }

    void answer(int index, const QJsonObject &status = snapshot()) { send(index, reply(index, status)); }

    QTcpServer server;
    QList<Request> requests;
    int connections = 0;
    bool listening = false;
};

void configureTiming(Client *client)
{
    client->setTiming({10000, 1000, 1000, 500, 500});
}

} // namespace

class VideoPlayerClientTests final : public QObject
{
    Q_OBJECT

private slots:
    void routesCommandsAndKeepsWindowQueuesIndependent()
    {
        Peer peer;
        QVERIFY(peer.listening);
        Client client;
        configureTiming(&client);
        QSignalSpy changed(&client, &Client::videoStatusChanged);
        QSignalSpy audioChanged(&client, &Client::statusChanged);
        QSignalSpy succeeded(&client, &Client::commandSucceeded);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        QCOMPARE(peer.requests.at(0).object.value("command").toString(), QStringLiteral("status"));
        peer.answer(0);
        QTRY_VERIFY(client.isReady());
        QVERIFY(client.hasStatus());
        QCOMPARE(client.videoStatus().windows.size(), 2);
        QCOMPARE(client.videoStatus().windows.at(0).playback.queue, QStringList{"/video/hall.mp4"});
        QCOMPARE(client.videoStatus().windows.at(1).playback.queue, QStringList{"C:/Video/foyer.mp4"});

        struct Command {
            QString name;
            QJsonObject arguments;
            std::function<QString()> send;
        };
        const QList<Command> commands{
            {"configureWindow", {{"name", "Фойе"}, {"screen", "screen-2"}, {"fullscreen", true}},
             [&] { return client.configureWindow("foyer", "Фойе", "screen-2", true); }},
            {"load", {{"paths", QJsonArray{"/video/updated.mp4"}}, {"startIndex", 0}, {"autoplay", true}},
             [&] { return client.load("foyer", {"/video/updated.mp4"}, 0, true); }},
            {"enqueue", {{"paths", QJsonArray{"/video/next.mp4"}}},
             [&] { return client.enqueue("foyer", {"/video/next.mp4"}); }},
            {"setSchedule", {{"schedule", QJsonObject{{"channels", QJsonArray{}}, {"adverts", QJsonArray{}}}}},
             [&] { return client.setSchedule("foyer", {{"channels", QJsonArray{}}, {"adverts", QJsonArray{}}}); }},
            {"schedule", {{"schedule", QJsonObject{{"channels", QJsonArray{}}, {"adverts", QJsonArray{}}}}},
             [&] { return client.startSchedule("foyer", {{"channels", QJsonArray{}}, {"adverts", QJsonArray{}}}); }},
            {"schedule", {}, [&] { return client.startSchedule("foyer"); }},
            {"playChannel", {{"name", "Канал"}, {"paths", QJsonArray{"/video/channel.mp4"}}, {"volume", 45}},
             [&] { return client.playChannel("foyer", "Канал", {"/video/channel.mp4"}, 45); }},
            {"play", {}, [&] { return client.play("foyer"); }},
            {"pause", {}, [&] { return client.pause("foyer"); }},
            {"stop", {}, [&] { return client.stop("foyer"); }},
            {"next", {}, [&] { return client.next("foyer"); }},
            {"previous", {}, [&] { return client.previous("foyer"); }},
            {"seek", {{"positionMs", 1234}}, [&] { return client.seek("foyer", 1234); }},
            {"volume", {{"value", 17}}, [&] { return client.setVolume("foyer", 17); }},
            {"mute", {{"value", true}}, [&] { return client.setMuted("foyer", true); }},
            {"repeat", {{"mode", "all"}}, [&] { return client.setRepeat("foyer", "all"); }},
            {"fullscreen", {{"value", false}}, [&] { return client.setFullscreen("foyer", false); }},
            {"clear", {}, [&] { return client.clear("foyer"); }},
            {"removeWindow", {}, [&] { return client.removeWindow("foyer"); }}
        };
        for (const auto &command : commands) {
            const int index = peer.requests.size();
            const QString id = command.send();
            QVERIFY(!id.isEmpty());
            QTRY_COMPARE(peer.requests.size(), index + 1);
            auto request = peer.requests.at(index).object;
            QCOMPARE(request.take("id").toString(), id);
            QCOMPARE(request.take("protocolVersion").toInt(), 1);
            QVERIFY(request.take("token").toString() == settingsFor(peer.port()).token);
            QJsonObject expected = command.arguments;
            expected.insert("command", command.name);
            expected.insert("windowId", "foyer");
            QCOMPARE(request, expected);
            peer.answer(index);
            QTRY_COMPARE(succeeded.size(), index + 1);
            QCOMPARE(client.videoStatus().windows.at(0).playback.queue, QStringList{"/video/hall.mp4"});
        }
        QCOMPARE(changed.size(), commands.size() + 1);
        QCOMPARE(audioChanged.size(), 0);
    }

    void authenticationFailureDoesNotRetry()
    {
        Peer peer;
        QVERIFY(peer.listening);
        Client client;
        client.setTiming({10000, 1000, 1000, 20, 20});
        QSignalSpy failed(&client, &Client::commandFailed);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        QJsonObject response = peer.reply(0);
        response.remove("status");
        response.insert("ok", false);
        response.insert("error", QJsonObject{{"code", "unauthorized"}, {"message", "Access denied"}});
        peer.send(0, response);
        QTRY_COMPARE(client.connectionState(), State::AuthenticationFailed);
        QVERIFY(!client.hasStatus());
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.first().at(2).toString(), QStringLiteral("unauthorized"));
        QTest::qWait(100);
        QCOMPARE(peer.connections, 1);
        QCOMPARE(peer.requests.size(), 1);
    }

    void scheduleStatusIsDecodedIndependentlyForEachWindow()
    {
        Peer peer;
        QVERIFY(peer.listening);
        Client client;
        configureTiming(&client);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        auto status = snapshot();
        auto windows = status.value("windows").toArray();
        auto window = windows.first().toObject();
        auto state = window.value("playback").toObject();
        state.insert("playbackMode", "schedule");
        state.insert("channelName", "Дневной канал");
        state.insert("scheduleAvailable", true);
        state.insert("scheduleError", "");
        window.insert("playback", state);
        windows[0] = window;
        status.insert("windows", windows);
        peer.answer(0, status);
        QTRY_VERIFY(client.isReady());
        const auto &first = client.videoStatus().windows.at(0).playback;
        QCOMPARE(first.playbackMode, QStringLiteral("schedule"));
        QCOMPARE(first.channelName, QStringLiteral("Дневной канал"));
        QVERIFY(first.scheduleAvailable);
        QVERIFY(first.scheduleError.isEmpty());
        const auto &second = client.videoStatus().windows.at(1).playback;
        QCOMPARE(second.playbackMode, QStringLiteral("manual"));
        QVERIFY(second.channelName.isEmpty());
        QVERIFY(!second.scheduleAvailable);
    }

    void malformedVideoStatus_data()
    {
        QTest::addColumn<QString>("kind");
        for (const char *kind : {"audio-status", "application", "displays", "duplicate-display",
                                "fractional-index", "duplicate-window", "missing-screen", "missing-playback",
                                "invalid-playback", "fullscreen", "unknown-actual-screen", "too-many-windows",
                                "persistence-error", "restore-error"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }

    void malformedVideoStatus()
    {
        QFETCH(QString, kind);
        Peer peer;
        QVERIFY(peer.listening);
        Client client;
        configureTiming(&client);
        QSignalSpy changed(&client, &Client::videoStatusChanged);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0);
        QTRY_VERIFY(client.isReady());
        QVERIFY(!client.requestStatus().isEmpty());
        QTRY_COMPARE(peer.requests.size(), 2);
        QJsonObject status = snapshot();
        if (kind == "audio-status") {
            status = playback();
        } else if (kind == "application") {
            status.insert("application", "MediaBoxPlayer");
        } else if (kind == "displays") {
            status.insert("displays", QJsonObject{});
        } else if (kind == "persistence-error") {
            status.insert("persistenceError", false);
        } else if (kind == "duplicate-display" || kind == "fractional-index") {
            auto displays = status.value("displays").toArray();
            if (kind == "duplicate-display") {
                displays.append(displays.first());
            } else {
                auto display = displays.first().toObject();
                display.insert("index", 0.5);
                displays[0] = display;
            }
            status.insert("displays", displays);
        } else {
            auto windows = status.value("windows").toArray();
            auto window = windows.first().toObject();
            if (kind == "duplicate-window") windows.append(window);
            if (kind == "missing-screen") window.remove("screen");
            if (kind == "missing-playback") window.remove("playback");
            if (kind == "fullscreen") window.insert("fullscreen", 1);
            if (kind == "restore-error") window.insert("restoreError", true);
            if (kind == "unknown-actual-screen") window.insert("actualScreen", "nonexistent");
            if (kind == "invalid-playback") {
                auto state = window.value("playback").toObject();
                state.insert("currentTrack", "/video/wrong.mp4");
                window.insert("playback", state);
            }
            if (kind == "too-many-windows") {
                for (int i = 2; i < 17; ++i) {
                    auto extra = window;
                    extra.insert("id", QStringLiteral("window-%1").arg(i));
                    windows.append(extra);
                }
            }
            windows[0] = window;
            status.insert("windows", windows);
        }
        peer.answer(1, status);
        QTRY_COMPARE(client.connectionState(), State::Reconnecting);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(client.videoStatus().windows.size(), 2);
        QCOMPARE(client.videoStatus().windows.at(0).playback.currentTrack, QStringLiteral("/video/hall.mp4"));
    }

    void detachedScreenAndSettingsSwitch()
    {
        Peer peer;
        Peer nextPeer;
        QVERIFY(peer.listening);
        QVERIFY(nextPeer.listening);
        Client client;
        configureTiming(&client);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        auto status = snapshot();
        auto displays = status.value("displays").toArray();
        displays.removeLast();
        status.insert("displays", displays);
        auto windows = status.value("windows").toArray();
        auto detached = windows.at(1).toObject();
        detached.insert("actualScreen", "");
        detached.insert("restoreError", "Missing video file");
        windows[1] = detached;
        status.insert("windows", windows);
        status.insert("persistenceError", "Changes were not saved");
        peer.answer(0, status);
        QTRY_VERIFY(client.isReady());
        QCOMPARE(client.videoStatus().windows.at(1).screen, QStringLiteral("screen-2"));
        QVERIFY(client.videoStatus().windows.at(1).actualScreen.isEmpty());
        QCOMPARE(client.videoStatus().windows.at(1).restoreError, QStringLiteral("Missing video file"));
        QCOMPARE(client.videoStatus().persistenceError, QStringLiteral("Changes were not saved"));
        client.connectToPlayer(settingsFor(nextPeer.port()));
        QVERIFY(!client.hasStatus());
        QVERIFY(client.videoStatus().windows.isEmpty());
        QTRY_COMPARE(nextPeer.requests.size(), 1);
        nextPeer.answer(0, {{"application", "MediaBoxVPlayer"}, {"displays", QJsonArray{}}, {"windows", QJsonArray{}}});
        QTRY_VERIFY(client.isReady());
        QVERIFY(client.hasStatus());
        QVERIFY(client.videoStatus().windows.isEmpty());
    }

    void invalidCommandsAreRejectedLocally()
    {
        Peer peer;
        QVERIFY(peer.listening);
        Client client;
        configureTiming(&client);
        QSignalSpy failed(&client, &Client::commandFailed);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0);
        QTRY_VERIFY(client.isReady());
        QVERIFY(client.play("").isEmpty());
        QVERIFY(client.play("bad id").isEmpty());
        QVERIFY(client.configureWindow("hall", " ", "", true).isEmpty());
        QVERIFY(client.configureWindow("hall", "Зал", QString(257, 'x'), true).isEmpty());
        QVERIFY(client.load("hall", {"relative.mp4"}).isEmpty());
        QVERIFY(client.seek("hall", -1).isEmpty());
        QVERIFY(client.setVolume("hall", 101).isEmpty());
        QVERIFY(client.setRepeat("hall", "random").isEmpty());
        QVERIFY(client.setSchedule("hall", {{"channels", QJsonArray{}}}).isEmpty());
        QVERIFY(client.startSchedule("hall", {{"adverts", QJsonArray{}}}).isEmpty());
        QVERIFY(client.playChannel("hall", " ", {"/video/a.mp4"}, 100).isEmpty());
        QVERIFY(client.playChannel("hall", "Канал", {"relative.mp4"}, 100).isEmpty());
        QVERIFY(client.playChannel("hall", "Канал", {"/video/a.mp4"}, -1).isEmpty());
        QCOMPARE(failed.size(), 13);
        QTest::qWait(30);
        QCOMPARE(peer.requests.size(), 1);
    }

    void statusSignalRemainsStableDuringSettingsSwitch()
    {
        Peer peer;
        Peer replacement;
        QVERIFY(peer.listening);
        QVERIFY(replacement.listening);
        Client client;
        configureTiming(&client);
        // The first direct observer resets the stored snapshot while the
        // second observer must still receive the complete confirmed snapshot.
        connect(&client, &Client::videoStatusChanged, &client, [&](const VideoPlayerStatus &) {
            client.connectToPlayer(settingsFor(replacement.port()));
        });
        QSignalSpy changed(&client, &Client::videoStatusChanged);
        client.connectToPlayer(settingsFor(peer.port()));
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0);
        QTRY_COMPARE(changed.size(), 1);
        QVERIFY(!client.hasStatus());
        QVERIFY(client.videoStatus().windows.isEmpty());
        const auto received = changed.first().first().value<VideoPlayerStatus>();
        QCOMPARE(received.windows.size(), 2);
        QCOMPARE(received.windows.first().playback.queue, QStringList{"/video/hall.mp4"});
        QTRY_COMPARE(replacement.requests.size(), 1);
    }
};

QTEST_GUILESS_MAIN(VideoPlayerClientTests)
#include "tst_vplayerclient.moc"
