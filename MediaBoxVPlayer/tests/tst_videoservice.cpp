#include "audiobackend.h"
#include "controlserver.h"
#include "videoservice.h"
#include "videowindow.h"
#include "schedulecore/schedulecompiler.h"
#include <QCryptographicHash>
#include <QDir>
#include <QUuid>

#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QVideoWidget>

using namespace MediaBox;

class FakeVideoBackend final : public AudioBackend
{
public:
    using AudioBackend::AudioBackend;

    void setSource(const QUrl &value) override { source = value; }
    void play() override
    {
        ++playCalls;
        emit stateChanged(State::Playing);
    }
    void pause() override { emit stateChanged(State::Paused); }
    void stop() override { emit stateChanged(State::Stopped); }
    void seek(qint64 value) override { emit positionChanged(value); }
    void setVolume(int value) override { volume = value; }
    void setMuted(bool value) override { muted = value; }
    void finish()
    {
        emit stateChanged(State::Stopped);
        emit finished();
    }

    QUrl source;
    int volume = -1;
    int playCalls = 0;
    bool muted = false;
};

class VideoServiceTests final : public QObject
{
    Q_OBJECT

private:
    static AudioBackend *createBackend(QVideoWidget *, QObject *parent)
    {
        return new FakeVideoBackend(parent);
    }

    static QString createVideo(const QTemporaryDir &directory, const QString &name)
    {
        const QString path = directory.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write("fake video") != 10)
            return {};
        return path;
    }

    static QJsonObject configureRequest(const QString &id, const QString &name = "Video",
                                        const QString &screen = {}, bool fullscreen = false)
    {
        return {{"command", "configureWindow"}, {"windowId", id}, {"name", name},
                {"screen", screen}, {"fullscreen", fullscreen}};
    }

    static QJsonObject command(VideoService &service, const QString &name,
                               const QString &windowId, QJsonObject arguments = {})
    {
        arguments.insert("command", name);
        arguments.insert("windowId", windowId);
        return service.execute(arguments);
    }

    static QJsonObject windowStatus(const VideoService &service, const QString &id)
    {
        for (const auto &value : service.status().value("windows").toArray()) {
            const auto window = value.toObject();
            if (window.value("id").toString() == id)
                return window;
        }
        return {};
    }

    static QJsonObject playback(const VideoService &service, const QString &id)
    {
        return windowStatus(service, id).value("playback").toObject();
    }

    static QJsonObject allDaySchedule(const QJsonArray &paths, const QString &order = QStringLiteral("sequential"))
    {
        const QJsonObject channel{{"id", "all-day"}, {"name", "Полные сутки"},
            {"start", "00:00"}, {"end", "00:00"}, {"untilDayOffset", 1},
            {"weekdays", "*"}, {"days", "*"}, {"months", "*"}, {"volume", 65},
            {"order", order}, {"paths", paths}};
        return {{"channels", QJsonArray{channel}}, {"adverts", QJsonArray{}}};
    }

    static bool put(const QString &path, const QByteArray &bytes)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    }

    static QJsonObject readSnapshot(const QJsonObject &reference)
    {
        QFile pointer(reference.value("activePath").toString());
        if (!pointer.open(QIODevice::ReadOnly)) return {};
        const auto active = QJsonDocument::fromJson(pointer.readAll()).object();
        QFile snapshot(QDir(QFileInfo(pointer).absolutePath()).filePath(active.value("snapshotPath").toString()));
        return snapshot.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(snapshot.readAll()).object() : QJsonObject{};
    }

    static QJsonObject writePublication(const QString &root, QJsonObject source, const QJsonObject &previous = {})
    {
        auto channel = source.value("channels").toArray().first().toObject();
        channel.insert("id", "00000000-0000-4000-8000-000000000111");
        source.insert("channels", QJsonArray{channel});
        QJsonObject document;
        QString error;
        if (!ScheduleCompiler::fromChannels(source, root, previous, &document, &error, QStringLiteral("video"))) return {};
        document.insert("publicationId", QUuid::createUuid().toString(QUuid::WithoutBraces));
        document.insert("revision", previous.value("revision").toInt() + 1);
        const QByteArray bytes = QJsonDocument(document).toJson();
        const QString relative = "snapshots/" + document.value("publicationId").toString() + ".json";
        const QJsonObject active{{"format", "mediabox.active"}, {"schemaVersion", 1},
            {"scheduleId", document.value("scheduleId")}, {"stationId", document.value("stationId")},
            {"publicationId", document.value("publicationId")}, {"revision", document.value("revision")},
            {"snapshotPath", relative}, {"sha256", QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())}};
        const QString directory = QDir(root).filePath("published");
        if (!QDir().mkpath(QDir(directory).filePath("snapshots"))
            || !put(QDir(directory).filePath(relative), bytes)
            || !put(QDir(directory).filePath("active.json"), QJsonDocument(active).toJson())) return {};
        return {{"activePath", QDir(directory).filePath("active.json")}, {"contentRoot", root}};
    }

    static QJsonObject savedWindow(const QString &id, const QJsonArray &paths)
    {
        auto definition = configureRequest(id);
        definition.remove("command");
        definition.insert("paths", paths);
        definition.insert("currentIndex", paths.isEmpty() ? -1 : 0);
        definition.insert("volumePercent", 100);
        definition.insert("muted", false);
        definition.insert("repeat", "off");
        definition.insert("playbackMode", "manual");
        return definition;
    }

private slots:
    void filePublicationsAreIsolatedReloadAndResumeOnlyScheduledWindows()
    {
        QTemporaryDir directory;
        const QString first = createVideo(directory, "first.mp4");
        const QString second = createVideo(directory, "second.mp4");
        const auto initial = writePublication(directory.path(), allDaySchedule({first}));
        QVERIFY(!initial.isEmpty());
        QString secondPublicationId;
        {
            VideoService service(directory.path(), createBackend);
            QVERIFY(service.execute(configureRequest("Main")).value("ok").toBool());
            QVERIFY(service.execute(configureRequest("main")).value("ok").toBool());
            const auto other = playback(service, "main");
            auto reply = command(service, "loadPublication", "Main", initial);
            QVERIFY2(reply.value("ok").toBool(), QJsonDocument(reply).toJson().constData());
            QVERIFY(playback(service, "Main").value("supportedCapabilities").toArray().contains("media.video.v1"));
            QVERIFY(playback(service, "Main").value("scheduleAvailable").toBool());
            QCOMPARE(playback(service, "Main").value("state").toString(), QString("stopped"));
            QVERIFY(command(service, "schedule", "Main").value("ok").toBool());
            QCOMPARE(playback(service, "Main").value("currentTrack").toString(), first);
            const auto previous = readSnapshot(initial);
            const auto updated = writePublication(directory.path(), allDaySchedule({second}), previous);
            QVERIFY(!updated.isEmpty());
            reply = command(service, "loadPublication", "Main", updated);
            QVERIFY2(reply.value("ok").toBool(), QJsonDocument(reply).toJson().constData());
            QCOMPARE(playback(service, "Main").value("currentTrack").toString(), first);
            QVERIFY(command(service, "next", "Main").value("ok").toBool());
            QCOMPARE(playback(service, "Main").value("currentTrack").toString(), second);
            QCOMPARE(playback(service, "main"), other);
            secondPublicationId = playback(service, "Main").value("publicationId").toString();
            QVERIFY(!secondPublicationId.isEmpty());
            QFile state(directory.filePath("windows.json"));
            QVERIFY(state.open(QIODevice::ReadOnly));
            const auto saved = QJsonDocument::fromJson(state.readAll()).object();
            QCOMPARE(saved.value("version").toInt(), 3);
            const auto item = saved.value("windows").toArray().first().toObject();
            QVERIFY(!item.contains("schedule"));
            QCOMPARE(item.value("publication").toObject(), initial);
        }
        QList<QPointer<FakeVideoBackend>> backends;
        VideoService restored(directory.path(), [&backends](QVideoWidget *, QObject *parent) {
            auto *backend = new FakeVideoBackend(parent); backends.append(backend); return backend;
        });
        QString error;
        QVERIFY2(restored.restore(&error), qPrintable(error));
        QCOMPARE(playback(restored, "Main").value("publicationId").toString(), secondPublicationId);
        QVERIFY(playback(restored, "Main").value("scheduleAvailable").toBool());
        QVERIFY(!playback(restored, "main").value("scheduleAvailable").toBool());
        QCOMPARE(backends.at(0)->playCalls, 1);
        QCOMPARE(backends.at(1)->playCalls, 0);
        QCOMPARE(playback(restored, "Main").value("currentTrack").toString(), second);
    }

    void rejectedInlineRequestsLeaveAcceptedPublicationUntouched()
    {
        QTemporaryDir directory;
        const auto reference = writePublication(directory.path(), allDaySchedule({createVideo(directory, "video.mp4")}));
        VideoService service(directory.path(), createBackend);
        QVERIFY(service.execute(configureRequest("main")).value("ok").toBool());
        QVERIFY(command(service, "loadPublication", "main", reference).value("ok").toBool());
        const auto accepted = playback(service, "main");
        for (const auto &entry : {qMakePair(QString("unknownCommand"), QJsonObject{}),
             qMakePair(QString("schedule"), QJsonObject{{"document", QJsonObject{}}}),
             qMakePair(QString("loadPublication"), QJsonObject{{"activePath", "relative.json"}, {"contentRoot", directory.path()}})}) {
            QVERIFY(!command(service, entry.first, "main", entry.second).value("ok").toBool());
            QCOMPARE(playback(service, "main"), accepted);
        }
    }

    void acceptedPublicationKeepsStoppedPreferenceAcrossRestart()
    {
        QTemporaryDir directory;
        const QString video = createVideo(directory, "video.mp4");
        auto reference = writePublication(directory.path(), allDaySchedule({video}));
        reference.insert("autoplay", true);
        {
            VideoService service(directory.path(), createBackend);
            QVERIFY(service.execute(configureRequest("main")).value("ok").toBool());
            QVERIFY(command(service, "loadPublication", "main", reference).value("ok").toBool());
            QVERIFY(command(service, "stop", "main").value("ok").toBool());
        }
        QPointer<FakeVideoBackend> backend;
        VideoService restored(directory.path(), [&backend](QVideoWidget *, QObject *parent) {
            backend = new FakeVideoBackend(parent); return backend.data();
        });
        QString error;
        QVERIFY2(restored.restore(&error), qPrintable(error));
        QCOMPARE(backend->playCalls, 0);
        QVERIFY(playback(restored, "main").value("scheduleAvailable").toBool());
        QCOMPARE(playback(restored, "main").value("state").toString(), QString("stopped"));
        QVERIFY(command(restored, "schedule", "main").value("ok").toBool());
        QCOMPARE(backend->playCalls, 1);
        QCOMPARE(backend->source.toLocalFile(), video);
    }

    void currentStateRejectsUnsupportedVersionAndOversizedQueuesBeforeCreatingWindows()
    {
        QTemporaryDir directory;
        const QByteArray unsupported = QJsonDocument(QJsonObject{{"version", 99}, {"windows", QJsonArray{}}}).toJson();
        QVERIFY(put(directory.filePath("windows.json"), unsupported));
        VideoService service(directory.path(), createBackend);
        QString error;
        QVERIFY(!service.restore(&error));
        QVERIFY(service.status().value("windows").toArray().isEmpty());
        QJsonArray paths, windows;
        for (int i = 0; i < 1000; ++i) paths.append(directory.filePath(QString(800, 'x')));
        for (int i = 0; i < 16; ++i) windows.append(savedWindow(QString::number(i), paths));
        const QByteArray oversized = QJsonDocument(QJsonObject{{"version", 3}, {"windows", windows}}).toJson();
        QVERIFY(oversized.size() > 8 * 1024 * 1024);
        QVERIFY(oversized.size() < 64 * 1024 * 1024);
        QVERIFY(put(directory.filePath("windows.json"), oversized));
        QVERIFY(!service.restore(&error));
        QVERIFY(service.status().value("windows").toArray().isEmpty());
        QFile saved(directory.filePath("windows.json"));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QCOMPARE(saved.readAll(), oversized);
    }

    void unavailableSavedDisplayRejectsScheduledAndChannelPlayback()
    {
        QTemporaryDir directory;
        const QString video = createVideo(directory, "video.mp4");
        const auto reference = writePublication(directory.path(), allDaySchedule({video}));
        auto definition = savedWindow("detached", {video});
        definition.insert("screen", "missing-test-display");
        definition.insert("publication", reference);
        QVERIFY(put(directory.filePath("windows.json"), QJsonDocument(QJsonObject{{"version", 3}, {"windows", QJsonArray{definition}}}).toJson()));
        QPointer<FakeVideoBackend> backend;
        VideoService service(directory.path(), [&backend](QVideoWidget *, QObject *parent) {
            backend = new FakeVideoBackend(parent); return backend.data();
        });
        QString error;
        QVERIFY2(service.restore(&error), qPrintable(error));
        auto autoplay = reference; autoplay.insert("autoplay", true);
        for (const auto &reply : {command(service, "schedule", "detached"),
             command(service, "loadPublication", "detached", autoplay),
             command(service, "playChannel", "detached", {{"name", "Канал"}, {"paths", QJsonArray{video}}, {"volume", 100}})}) {
            QVERIFY(!reply.value("ok").toBool());
            QCOMPARE(reply.value("error").toObject().value("code").toString(), QStringLiteral("unknown_screen"));
        }
        QCOMPARE(backend->playCalls, 0);
        QVERIFY(!service.window("detached")->isVisible());
    }
    void windowsHaveIndependentPlaybackAndQueues()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createVideo(directory, QStringLiteral("Первое видео.mp4"));
        const QString second = createVideo(directory, QStringLiteral("second.mp4"));
        const QString other = createVideo(directory, QStringLiteral("other.mp4"));
        QVERIFY(!first.isEmpty() && !second.isEmpty() && !other.isEmpty());
        QList<QPointer<FakeVideoBackend>> backends;
        VideoService service(directory.path(), [&backends](QVideoWidget *, QObject *parent) {
            auto *backend = new FakeVideoBackend(parent);
            backends.append(backend);
            return backend;
        });
        QVERIFY(service.execute(configureRequest("left", "Left screen")).value("ok").toBool());
        QVERIFY(service.execute(configureRequest("right", "Right screen")).value("ok").toBool());
        QCOMPARE(backends.size(), 2);
        QVERIFY(service.window("left") != service.window("right"));
        QVERIFY(service.window("left")->videoWidget() != service.window("right")->videoWidget());

        QVERIFY(command(service, "load", "left", {
            {"paths", QJsonArray{first, second}}, {"autoplay", true}}).value("ok").toBool());
        QVERIFY(command(service, "load", "right", {
            {"paths", QJsonArray{other}}, {"autoplay", true}}).value("ok").toBool());
        QVERIFY(command(service, "volume", "left", {{"value", 23}}).value("ok").toBool());
        QVERIFY(command(service, "volume", "right", {{"value", 67}}).value("ok").toBool());
        QVERIFY(command(service, "repeat", "left", {{"mode", "all"}}).value("ok").toBool());
        QVERIFY(command(service, "repeat", "right", {{"mode", "one"}}).value("ok").toBool());
        QVERIFY(command(service, "mute", "right", {{"value", true}}).value("ok").toBool());
        QCOMPARE(backends.at(0)->volume, 23);
        QCOMPARE(backends.at(1)->volume, 67);
        QVERIFY(!backends.at(0)->muted);
        QVERIFY(backends.at(1)->muted);
        QCOMPARE(playback(service, "left").value("queue").toArray(), (QJsonArray{first, second}));
        QCOMPARE(playback(service, "right").value("queue").toArray(), QJsonArray{other});
        QCOMPARE(playback(service, "left").value("repeat").toString(), QStringLiteral("all"));
        QCOMPARE(playback(service, "right").value("repeat").toString(), QStringLiteral("one"));

        const QJsonObject rightBefore = playback(service, "right");
        backends.at(0)->finish();
        QTRY_COMPARE(playback(service, "left").value("currentIndex").toInt(), 1);
        QCOMPARE(backends.at(0)->source.toLocalFile(), second);
        QCOMPARE(playback(service, "right"), rightBefore);
        QVERIFY(command(service, "pause", "left").value("ok").toBool());
        QCOMPARE(playback(service, "left").value("state").toString(), QStringLiteral("paused"));
        QCOMPARE(playback(service, "right").value("state").toString(), QStringLiteral("playing"));
        QVERIFY(command(service, "clear", "left").value("ok").toBool());
        QVERIFY(playback(service, "left").value("queue").toArray().isEmpty());
        QCOMPARE(playback(service, "right"), rightBefore);
    }

    void configuringAnExistingWindowPreservesPlayback()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString video = createVideo(directory, "video.mp4");
        QVERIFY(!video.isEmpty());
        VideoService service(directory.path(), createBackend);
        QVERIFY(service.execute(configureRequest("main")).value("ok").toBool());
        QVERIFY(command(service, "load", "main", {
            {"paths", QJsonArray{video}}, {"autoplay", true}}).value("ok").toBool());
        VideoWindow *window = service.window("main");
        const auto before = playback(service, "main");
        QVERIFY(service.execute(configureRequest("main", "Renamed", {}, true)).value("ok").toBool());
        QCOMPARE(service.window("main"), window);
        QCOMPARE(playback(service, "main"), before);
        QCOMPARE(windowStatus(service, "main").value("name").toString(), QStringLiteral("Renamed"));
        QVERIFY(windowStatus(service, "main").value("fullscreen").toBool());
    }

    void invalidConfigurationAndTargetingLeaveWindowsUntouched()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        VideoService service(directory.path(), createBackend);
        QVERIFY(service.execute(configureRequest("main", "Original")).value("ok").toBool());
        const QJsonObject before = service.status();
        QPointer<VideoWindow> original = service.window("main");
        QList<QJsonObject> requests;
        for (const QJsonObject &invalidField : {
                 QJsonObject{{"name", "   "}}, QJsonObject{{"name", QString(129, 'a')}},
                 QJsonObject{{"fullscreen", "yes"}}, QJsonObject{{"screen", 12}},
                 QJsonObject{{"windowId", ""}}, QJsonObject{{"windowId", "bad/id"}},
                 QJsonObject{{"windowId", "main\n"}}, QJsonObject{{"windowId", "main\r\n"}},
                 QJsonObject{{"windowId", QString(65, 'a')}}, QJsonObject{{"unexpected", true}}}) {
            auto request = configureRequest("main", "Changed");
            for (auto it = invalidField.begin(); it != invalidField.end(); ++it)
                request.insert(it.key(), it.value());
            requests.append(request);
        }
        auto incomplete = configureRequest("main");
        incomplete.remove("screen");
        requests.append(incomplete);
        requests.append({{"command", "clear"}});
        requests.append({{"command", "clear"}, {"windowId", "main\n"}});
        requests.append({{"command", "clear"}, {"windowId", "unknown"}});
        requests.append({{"command", "removeWindow"}, {"windowId", "unknown"}});
        requests.append({{"command", "fullscreen"}, {"windowId", "main"}, {"value", 1}});
        for (const auto &request : requests) {
            const auto reply = service.execute(request);
            QVERIFY2(!reply.value("ok").toBool(), QJsonDocument(request).toJson().constData());
            QVERIFY(reply.value("error").isObject());
            QCOMPARE(service.status(), before);
            QCOMPARE(service.window("main"), original.data());
        }
    }

    void screenSelectionValidatesBeforeCreatingOrMovingWindows()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        VideoService service(directory.path(), createBackend);
        const auto displays = service.status().value("displays").toArray();
        QVERIFY(!displays.isEmpty());
        const QString selected = displays.first().toObject().value("id").toString();
        QVERIFY(!selected.isEmpty());
        QVERIFY(service.execute(configureRequest("main", "Screen", selected)).value("ok").toBool());
        QCOMPARE(windowStatus(service, "main").value("screen").toString(), selected);
        QCOMPARE(windowStatus(service, "main").value("actualScreen").toString(), selected);
        const QJsonObject before = service.status();
        for (const QString &id : {QStringLiteral("main"), QStringLiteral("new")}) {
            const auto reply = service.execute(configureRequest(id, "Moved", "missing-test-display"));
            QVERIFY(!reply.value("ok").toBool());
            QCOMPARE(reply.value("error").toObject().value("code").toString(), QStringLiteral("unknown_screen"));
            QCOMPARE(service.status(), before);
        }
        QVERIFY(!service.window("new"));
    }

    void windowLimitAndRemovalReleaseResources()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QList<QPointer<FakeVideoBackend>> backends;
        VideoService service(directory.path(), [&backends](QVideoWidget *, QObject *parent) {
            auto *backend = new FakeVideoBackend(parent);
            backends.append(backend);
            return backend;
        });
        for (int i = 0; i < 16; ++i) {
            const auto configured = service.execute(configureRequest(QString::number(i)));
            QVERIFY2(configured.value("ok").toBool(), qPrintable(QString::fromUtf8(
                QJsonDocument(configured).toJson(QJsonDocument::Compact))));
        }
        QCOMPARE(service.status().value("windows").toArray().size(), 16);
        const QJsonObject before = service.status();
        QVERIFY(!service.execute(configureRequest("overflow")).value("ok").toBool());
        QCOMPARE(service.status(), before);
        QCOMPARE(backends.size(), 16);
        // Updating a window at the limit must not consume another slot.
        const auto renamed = service.execute(configureRequest("0", "Renamed"));
        QVERIFY2(renamed.value("ok").toBool(), qPrintable(QString::fromUtf8(
            QJsonDocument(renamed).toJson(QJsonDocument::Compact))));
        QCOMPARE(backends.size(), 16);
        QPointer<VideoWindow> removed = service.window("0");
        const auto removal = command(service, "removeWindow", "0");
        QVERIFY2(removal.value("ok").toBool(), qPrintable(QString::fromUtf8(
            QJsonDocument(removal).toJson(QJsonDocument::Compact))));
        QVERIFY(!service.window("0"));
        QTRY_VERIFY(removed.isNull());
        QTRY_VERIFY(backends.at(0).isNull());
        QCOMPARE(service.status().value("windows").toArray().size(), 15);
        const auto replacement = service.execute(configureRequest("replacement"));
        QVERIFY2(replacement.value("ok").toBool(), qPrintable(QString::fromUtf8(
            QJsonDocument(replacement).toJson(QJsonDocument::Compact))));
        QCOMPARE(service.status().value("windows").toArray().size(), 16);
    }

    void restorePreservesEachPlaylistWithoutStartingPlayback()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createVideo(directory, "first.mp4");
        const QString second = createVideo(directory, "second.mp4");
        QVERIFY(!first.isEmpty() && !second.isEmpty());
        {
            VideoService service(directory.path(), createBackend);
            QVERIFY(service.execute(configureRequest("main", "Main", {}, true)).value("ok").toBool());
            QVERIFY(service.execute(configureRequest("other", "Other")).value("ok").toBool());
            QVERIFY(service.execute(configureRequest("removed")).value("ok").toBool());
            QVERIFY(command(service, "removeWindow", "removed").value("ok").toBool());
            QVERIFY(command(service, "load", "main", {
                {"paths", QJsonArray{first, second}}, {"autoplay", true}}).value("ok").toBool());
            QVERIFY(command(service, "load", "other", {
                {"paths", QJsonArray{second}}, {"autoplay", true}}).value("ok").toBool());
            QVERIFY(command(service, "volume", "main", {{"value", 19}}).value("ok").toBool());
            QVERIFY(command(service, "repeat", "main", {{"mode", "all"}}).value("ok").toBool());
            QVERIFY(command(service, "volume", "other", {{"value", 81}}).value("ok").toBool());
            QVERIFY(command(service, "repeat", "other", {{"mode", "one"}}).value("ok").toBool());
            QVERIFY(command(service, "mute", "other", {{"value", true}}).value("ok").toBool());
        }
        QList<QPointer<FakeVideoBackend>> backends;
        VideoService restored(directory.path(), [&backends](QVideoWidget *, QObject *parent) {
            auto *backend = new FakeVideoBackend(parent);
            backends.append(backend);
            return backend;
        });
        QString error;
        QVERIFY2(restored.restore(&error), qPrintable(error));
        QCOMPARE(restored.status().value("windows").toArray().size(), 2);
        QVERIFY(!restored.window("removed"));
        QCOMPARE(windowStatus(restored, "main").value("name").toString(), QStringLiteral("Main"));
        QVERIFY(windowStatus(restored, "main").value("fullscreen").toBool());
        QVERIFY(!windowStatus(restored, "other").value("fullscreen").toBool());
        QCOMPARE(playback(restored, "main").value("queue").toArray(), (QJsonArray{first, second}));
        QCOMPARE(playback(restored, "other").value("queue").toArray(), QJsonArray{second});
        QCOMPARE(playback(restored, "main").value("volumePercent").toInt(), 19);
        QCOMPARE(playback(restored, "other").value("volumePercent").toInt(), 81);
        QCOMPARE(playback(restored, "main").value("repeat").toString(), QStringLiteral("all"));
        QCOMPARE(playback(restored, "other").value("repeat").toString(), QStringLiteral("one"));
        QVERIFY(playback(restored, "other").value("muted").toBool());
        for (const QString &id : {QStringLiteral("main"), QStringLiteral("other")}) {
            QCOMPARE(playback(restored, id).value("state").toString(), QStringLiteral("stopped"));
            QVERIFY(!playback(restored, id).value("playbackRequested").toBool());
        }
        for (const auto &backend : backends)
            QCOMPARE(backend->playCalls, 0);
    }

    void saveFailureReportsRunningChangesAndClearsAfterRecovery()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString dataDirectory = directory.filePath("state");
        // A regular file where the directory should be fails even for root,
        // unlike permission-based tests in a privileged CI environment.
        QFile obstruction(dataDirectory);
        QVERIFY(obstruction.open(QIODevice::WriteOnly));
        obstruction.close();
        VideoService service(dataDirectory, createBackend);
        const auto failed = service.execute(configureRequest("main", "Unsaved window"));
        QVERIFY(!failed.value("ok").toBool());
        QCOMPARE(failed.value("error").toObject().value("code").toString(), QStringLiteral("persistence_error"));
        QVERIFY(!failed.value("status").toObject().value("persistenceError").toString().isEmpty());
        QCOMPARE(failed.value("status").toObject(), service.status());
        QVERIFY(service.window("main"));
        QCOMPARE(windowStatus(service, "main").value("name").toString(), QStringLiteral("Unsaved window"));

        const auto changed = command(service, "volume", "main", {{"value", 32}});
        QVERIFY(!changed.value("ok").toBool());
        QCOMPARE(changed.value("status").toObject(), service.status());
        QCOMPARE(playback(service, "main").value("volumePercent").toInt(), 32);
        QVERIFY(QFile::remove(dataDirectory));
        const auto recovered = command(service, "repeat", "main", {{"mode", "all"}});
        QVERIFY(recovered.value("ok").toBool());
        QVERIFY(recovered.value("status").toObject().value("persistenceError").toString().isEmpty());
        QCOMPARE(recovered.value("status").toObject(), service.status());

        VideoService restored(dataDirectory, createBackend);
        QString error;
        QVERIFY2(restored.restore(&error), qPrintable(error));
        QCOMPARE(restored.status().value("windows").toArray().size(), 1);
        QCOMPARE(windowStatus(restored, "main").value("name").toString(), QStringLiteral("Unsaved window"));
        QCOMPARE(playback(restored, "main").value("volumePercent").toInt(), 32);
        QCOMPARE(playback(restored, "main").value("repeat").toString(), QStringLiteral("all"));
    }

    void restoreSkipsUnavailableFilesAndKeepsOtherVideosStopped()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString missing = createVideo(directory, "removed.mp4");
        const QString selected = createVideo(directory, "selected.mp4");
        const QString last = createVideo(directory, "last.mp4");
        QVERIFY(!missing.isEmpty() && !selected.isEmpty() && !last.isEmpty());
        {
            VideoService service(directory.path(), createBackend);
            QVERIFY(service.execute(configureRequest("mixed")).value("ok").toBool());
            QVERIFY(service.execute(configureRequest("unavailable")).value("ok").toBool());
            QVERIFY(service.execute(configureRequest("healthy")).value("ok").toBool());
            QVERIFY(command(service, "load", "mixed", {
                {"paths", QJsonArray{missing, selected, last}}, {"startIndex", 1},
                {"autoplay", true}}).value("ok").toBool());
            QVERIFY(command(service, "load", "unavailable", {
                {"paths", QJsonArray{missing}}}).value("ok").toBool());
            QVERIFY(command(service, "load", "healthy", {
                {"paths", QJsonArray{last}}}).value("ok").toBool());
        }
        QVERIFY(QFile::remove(missing));
        QList<QPointer<FakeVideoBackend>> backends;
        VideoService restored(directory.path(), [&backends](QVideoWidget *, QObject *parent) {
            auto *backend = new FakeVideoBackend(parent);
            backends.append(backend);
            return backend;
        });
        QString error;
        QVERIFY2(restored.restore(&error), qPrintable(error));
        QCOMPARE(restored.status().value("windows").toArray().size(), 3);
        QCOMPARE(playback(restored, "mixed").value("queue").toArray(), (QJsonArray{selected, last}));
        QCOMPARE(playback(restored, "mixed").value("currentIndex").toInt(-1), 0);
        QCOMPARE(playback(restored, "mixed").value("currentTrack").toString(), selected);
        QVERIFY(playback(restored, "unavailable").value("queue").toArray().isEmpty());
        QCOMPARE(playback(restored, "unavailable").value("currentIndex").toInt(), -1);
        QCOMPARE(playback(restored, "healthy").value("queue").toArray(), QJsonArray{last});
        for (const QString &id : {QStringLiteral("mixed"), QStringLiteral("unavailable")})
            QVERIFY(windowStatus(restored, id).value("restoreError").toString().contains(missing));
        QVERIFY(windowStatus(restored, "healthy").value("restoreError").toString().isEmpty());
        for (const QString &id : {QStringLiteral("mixed"), QStringLiteral("unavailable"), QStringLiteral("healthy")}) {
            QCOMPARE(playback(restored, id).value("state").toString(), QStringLiteral("stopped"));
            QVERIFY(!playback(restored, id).value("playbackRequested").toBool());
        }
        for (const auto &backend : backends)
            QCOMPARE(backend->playCalls, 0);
        QVERIFY(command(restored, "load", "mixed", {{"paths", QJsonArray{last}}}).value("ok").toBool());
        QVERIFY(windowStatus(restored, "mixed").value("restoreError").toString().isEmpty());
        QVERIFY(command(restored, "clear", "unavailable").value("ok").toBool());
        QVERIFY(windowStatus(restored, "unavailable").value("restoreError").toString().isEmpty());
    }

    void authenticatedTcpRoutesCommandsToSelectedWindow()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString video = createVideo(directory, "video.mp4");
        QVERIFY(!video.isEmpty());
        VideoService service(directory.path(), createBackend);
        const QByteArray token(64, 'd');
        ControlServer server([&service](const QJsonObject &request) {
            return service.execute(request);
        }, token);
        QString error;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, server.port());
        QTRY_COMPARE(socket.state(), QAbstractSocket::ConnectedState);
        int requestId = 0;
        const auto exchange = [&](QJsonObject request, bool authenticated = true) {
            request.insert("id", ++requestId);
            request.insert("protocolVersion", 1);
            request.insert("token", authenticated ? QString::fromLatin1(token) : QStringLiteral("wrong"));
            socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
            QElapsedTimer timer;
            timer.start();
            while (!socket.canReadLine() && timer.elapsed() < 2000)
                QTest::qWait(1);
            return QJsonDocument::fromJson(socket.readLine()).object();
        };
        const auto denied = exchange(configureRequest("main"), false);
        QCOMPARE(denied.value("error").toObject().value("code").toString(), QStringLiteral("unauthorized"));
        QVERIFY(!service.window("main"));
        QVERIFY(exchange(configureRequest("main")).value("ok").toBool());
        QVERIFY(exchange(configureRequest("other")).value("ok").toBool());
        const auto otherBefore = playback(service, "other");
        const auto loaded = exchange({{"command", "load"}, {"windowId", "main"},
                                     {"paths", QJsonArray{video}}, {"autoplay", true}});
        QVERIFY(loaded.value("ok").toBool());
        QCOMPARE(loaded.value("id").toInt(), requestId);
        QCOMPARE(loaded.value("protocolVersion").toInt(), 1);
        QCOMPARE(playback(service, "main").value("state").toString(), QStringLiteral("playing"));
        QCOMPARE(playback(service, "other"), otherBefore);

        socket.disconnectFromHost();
        QTRY_COMPARE(socket.state(), QAbstractSocket::UnconnectedState);
        socket.connectToHost(QHostAddress::LocalHost, server.port());
        QTRY_COMPARE(socket.state(), QAbstractSocket::ConnectedState);
        const auto reconnected = exchange({{"command", "status"}});
        QVERIFY(reconnected.value("ok").toBool());
        QCOMPARE(reconnected.value("status").toObject(), service.status());
        QCOMPARE(playback(service, "main").value("state").toString(), QStringLiteral("playing"));
        QVERIFY(exchange({{"command", "pause"}, {"windowId", "main"}}).value("ok").toBool());
        QCOMPARE(playback(service, "main").value("state").toString(), QStringLiteral("paused"));
        QCOMPARE(playback(service, "other"), otherBefore);
    }

    void closingOneWindowStopsItAndAutoplayShowsItAgain()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createVideo(directory, "first.mp4");
        const QString replacement = createVideo(directory, "replacement.mp4");
        QVERIFY(!first.isEmpty() && !replacement.isEmpty());
        VideoService service(directory.path(), createBackend);
        QVERIFY(service.execute(configureRequest("main")).value("ok").toBool());
        QVERIFY(service.execute(configureRequest("other")).value("ok").toBool());
        for (const QString &id : {QStringLiteral("main"), QStringLiteral("other")}) {
            QVERIFY(command(service, "load", id, {
                {"paths", QJsonArray{first}}, {"autoplay", true}}).value("ok").toBool());
            QVERIFY(service.window(id)->isVisible());
            QCOMPARE(playback(service, id).value("state").toString(), QStringLiteral("playing"));
        }
        const auto otherBefore = windowStatus(service, "other");
        QPointer<VideoWindow> window = service.window("main");
        // close() must accept the event so it cannot veto QApplication's quit.
        QVERIFY(window->close());
        QVERIFY(window);
        QVERIFY(!window->isVisible());
        QCOMPARE(service.window("main"), window.data());
        QCOMPARE(playback(service, "main").value("state").toString(), QStringLiteral("stopped"));
        QVERIFY(!playback(service, "main").value("playbackRequested").toBool());
        QCOMPARE(playback(service, "main").value("queue").toArray(), QJsonArray{first});
        QVERIFY(windowStatus(service, "main").value("actualScreen").toString().isEmpty());
        QCOMPARE(windowStatus(service, "other"), otherBefore);

        QVERIFY(command(service, "load", "main", {
            {"paths", QJsonArray{replacement}}, {"autoplay", true}}).value("ok").toBool());
        QCOMPARE(service.window("main"), window.data());
        QVERIFY(window->isVisible());
        QCOMPARE(playback(service, "main").value("state").toString(), QStringLiteral("playing"));
        QCOMPARE(playback(service, "main").value("queue").toArray(), QJsonArray{replacement});
        QVERIFY(!windowStatus(service, "main").value("actualScreen").toString().isEmpty());
        QCOMPARE(windowStatus(service, "other"), otherBefore);
    }

    void doubleClickOnVideoTogglesOnlyItsWindowFullscreen()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        VideoService service(directory.path(), createBackend);
        QVERIFY(service.execute(configureRequest("main", "Main", {}, true)).value("ok").toBool());
        QVERIFY(service.execute(configureRequest("other")).value("ok").toBool());
        VideoWindow *window = service.window("main");
        QVERIFY(window);
        QVERIFY(window->requestedFullscreen());
        QTRY_VERIFY(window->isFullScreen());
        const auto otherBefore = windowStatus(service, "other");
        QTest::mouseDClick(window->videoWidget(), Qt::LeftButton);
        QVERIFY(!window->requestedFullscreen());
        QTRY_VERIFY(!window->isFullScreen());
        QVERIFY(!windowStatus(service, "main").value("fullscreen").toBool());
        QTest::mouseDClick(window->videoWidget(), Qt::LeftButton);
        QVERIFY(window->requestedFullscreen());
        QTRY_VERIFY(window->isFullScreen());
        QVERIFY(windowStatus(service, "main").value("fullscreen").toBool());
        QCOMPARE(windowStatus(service, "other"), otherBefore);
        QVERIFY(command(service, "fullscreen", "main", {{"value", false}}).value("ok").toBool());
        QVERIFY(!window->requestedFullscreen());
        QTRY_VERIFY(!window->isFullScreen());
    }
};

QTEST_MAIN(VideoServiceTests)
#include "tst_videoservice.moc"
