#include "audiobackend.h"
#include "playerengine.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrl>

using namespace MediaBox;

class FakeAudioBackend final : public AudioBackend
{
public:
    using AudioBackend::AudioBackend;

    void setSource(const QUrl &url) override
    {
        ++sourceCalls;
        source = url;
        position = 0;
    }

    void play() override
    {
        ++playCalls;
        playedSources.append(source.toLocalFile());
        if (rejectEveryTrack) {
            QTimer::singleShot(0, this, [this] {
                emit errorOccurred(QStringLiteral("Cannot decode test audio"));
            });
        }
    }

    void pause() override { ++pauseCalls; }

    void stop() override
    {
        ++stopCalls;
        position = 0;
        emit stateChanged(State::Stopped);
    }

    void seek(qint64 value) override
    {
        position = value;
        emit positionChanged(value);
    }

    void setVolume(int value) override { volume = value; }
    void setMuted(bool value) override { muted = value; }

    void confirmPlaying() { emit stateChanged(State::Playing); }
    void confirmPaused() { emit stateChanged(State::Paused); }
    void reportDuration(qint64 value) { emit durationChanged(value); }
    void reportPosition(qint64 value)
    {
        position = value;
        emit positionChanged(value);
    }
    void finish()
    {
        emit stateChanged(State::Stopped);
        emit finished();
    }
    void fail() { emit errorOccurred(QStringLiteral("Cannot decode test audio")); }

    QUrl source;
    QStringList playedSources;
    int playCalls = 0;
    int sourceCalls = 0;
    int pauseCalls = 0;
    int stopCalls = 0;
    qint64 position = 0;
    int volume = -1;
    bool muted = false;
    bool rejectEveryTrack = false;
};

class PlayerEngineTest final : public QObject
{
    Q_OBJECT

private:
    static QString createTrack(const QTemporaryDir &directory, const QString &name)
    {
        const QString path = directory.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write("fake audio") != 10)
            return {};
        file.close();
        return path;
    }

    static QJsonObject command(PlayerEngine &engine, const QString &name,
                               QJsonObject arguments = {})
    {
        arguments.insert(QStringLiteral("command"), name);
        return engine.execute(arguments);
    }

    static QJsonObject load(PlayerEngine &engine, const QStringList &paths,
                            bool autoplay = false, int startIndex = 0)
    {
        return command(engine, QStringLiteral("load"), {
            {QStringLiteral("paths"), QJsonArray::fromStringList(paths)},
            {QStringLiteral("autoplay"), autoplay},
            {QStringLiteral("startIndex"), startIndex}
        });
    }

    static QString state(const PlayerEngine &engine)
    {
        return engine.status().value(QStringLiteral("state")).toString();
    }

    static void settle()
    {
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }

    static QDateTime at(const QString &time)
    {
        return QDateTime::fromString(QStringLiteral("2026-10-04T") + time + QLatin1Char('Z'), Qt::ISODate);
    }

    static QJsonObject channel(const QString &id, const QString &name, const QString &start,
                               const QString &end, const QStringList &paths, int volume = 75)
    {
        return {{"id", id}, {"name", name}, {"start", start}, {"end", end},
                {"weekdays", "*"}, {"days", "*"}, {"months", "*"}, {"volume", volume},
                {"paths", QJsonArray::fromStringList(paths)}};
    }

    static QJsonObject advert(const QString &id, const QString &name, const QString &timing,
                              const QStringList &paths, int volume = 60)
    {
        return {{"id", id}, {"name", name}, {"hours", "*"}, {"weekdays", "*"},
                {"from", "2026-10-04"}, {"until", "2026-10-04"}, {"timing", timing},
                {"volume", volume}, {"paths", QJsonArray::fromStringList(paths)}};
    }

    static QJsonObject schedule(const QJsonArray &channels, const QJsonArray &adverts = {})
    {
        return {{"channels", channels}, {"adverts", adverts}};
    }

private slots:
    void playChannelStartsWholeQueueAtomicallyAndOverridesSchedule()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString scheduled = createTrack(directory, "scheduled.wav");
        const QString first = createTrack(directory, "first.wav");
        const QString second = createTrack(directory, "second.wav");
        QDateTime now = at("10:00:00");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        QVERIFY(command(engine, "schedule", {{"schedule", schedule({channel("scheduled", "По плану", "09:00", "12:00", {scheduled})})}}).value("ok").toBool());
        const QJsonObject before = engine.status();
        const int playsBefore = backend.playCalls;
        const QList<QJsonObject> invalid = {
            {{"name", "Канал"}, {"paths", QJsonArray{}}, {"volume", 50}},
            {{"name", "Канал"}, {"paths", QJsonArray{first, directory.filePath("missing.wav")}}, {"volume", 50}},
            {{"name", " "}, {"paths", QJsonArray{first}}, {"volume", 50}},
            {{"name", "Канал"}, {"paths", QJsonArray{first}}, {"volume", 100.5}}
        };
        for (const auto &arguments : invalid) {
            QVERIFY(!command(engine, "playChannel", arguments).value("ok").toBool());
            QCOMPARE(engine.status(), before);
            QCOMPARE(backend.playCalls, playsBefore);
        }
        QVERIFY(command(engine, "playChannel", {{"name", "Любимый канал"}, {"paths", QJsonArray{first, second}}, {"volume", 42}}).value("ok").toBool());
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("manual"));
        QCOMPARE(engine.status().value("channelName").toString(), QStringLiteral("Любимый канал"));
        QCOMPARE(engine.status().value("queue").toArray(), (QJsonArray{first, second}));
        QCOMPARE(engine.status().value("repeat").toString(), QStringLiteral("all"));
        QCOMPARE(backend.volume, 42);
        QCOMPARE(backend.source.toLocalFile(), first);
        backend.finish();
        settle();
        QCOMPARE(backend.source.toLocalFile(), second);
        backend.finish();
        settle();
        QCOMPARE(backend.source.toLocalFile(), first);
        now = at("11:00:00");
        engine.evaluateSchedule(now);
        QCOMPARE(backend.source.toLocalFile(), first);
    }

    void scheduleSnapshotLoadsWithoutChangingModeAndActivationIsAtomic()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString track = createTrack(directory, "track.wav");
        QDateTime now = at("10:00:00");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        QVERIFY(!command(engine, "schedule").value("ok").toBool());
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("manual"));
        QVERIFY(!engine.status().value("scheduleAvailable").toBool());
        const QJsonObject snapshot = schedule({channel("first", "Первый", "09:00", "12:00", {track})});
        QVERIFY(command(engine, "setSchedule", {{"schedule", snapshot}}).value("ok").toBool());
        QCOMPARE(backend.playCalls, 0);
        QVERIFY(engine.status().value("scheduleAvailable").toBool());
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("manual"));
        auto invalidChannel = channel("bad", "Ошибка", "09:00", "12:00", {track});
        invalidChannel.insert("weekdays", "7");
        const QJsonObject before = engine.status();
        QVERIFY(!command(engine, "schedule", {{"schedule", schedule({invalidChannel})}}).value("ok").toBool());
        QCOMPARE(engine.status(), before);
        QVERIFY(command(engine, "schedule").value("ok").toBool());
        QCOMPARE(backend.source.toLocalFile(), track);
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("schedule"));
        QVERIFY(command(engine, "stop").value("ok").toBool());
        const int stoppedPlays = backend.playCalls;
        engine.evaluateSchedule(now.addSecs(1));
        QCOMPARE(backend.playCalls, stoppedPlays);
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("manual"));
    }

    void scheduleHonoursBoundariesGapsAndDoesNotRestartCurrentChannel()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, "first.wav");
        const QString second = createTrack(directory, "second.wav");
        QDateTime now = at("08:59:59");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        QVERIFY(command(engine, "schedule", {{"schedule", schedule({channel("first", "Утро", "09:00", "10:00", {first}), channel("second", "День", "10:00", "11:00", {second})})}}).value("ok").toBool());
        QCOMPARE(backend.playCalls, 0);
        now = at("09:00:00");
        engine.evaluateSchedule(now);
        QCOMPARE(backend.playCalls, 1);
        QCOMPARE(backend.source.toLocalFile(), first);
        backend.confirmPlaying();
        backend.reportPosition(1234);
        engine.evaluateSchedule(at("09:59:59"));
        QCOMPARE(backend.playCalls, 1);
        QCOMPARE(engine.status().value("positionMs").toInteger(), 1234);
        now = at("10:00:00");
        engine.evaluateSchedule(now);
        QCOMPARE(backend.playCalls, 2);
        QCOMPARE(backend.source.toLocalFile(), second);
        now = at("11:00:00");
        engine.evaluateSchedule(now);
        QCOMPARE(state(engine), QStringLiteral("stopped"));
        QVERIFY(engine.status().value("queue").toArray().isEmpty());
        QVERIFY(engine.status().value("channelName").toString().isEmpty());
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("schedule"));
        QCOMPARE(backend.playCalls, 2);
    }

    void scheduleUpdatesPreservePlaybackWhenQueueIsUnchanged()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, "first.wav");
        const QString second = createTrack(directory, "second.wav");
        QDateTime now = at("10:00:00");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        auto rule = channel("first", "Первый", "09:00", "12:00", {first});
        QVERIFY(command(engine, "schedule", {{"schedule", schedule({rule})}}).value("ok").toBool());
        backend.confirmPlaying();
        backend.reportPosition(2345);
        const int sourceCalls = backend.sourceCalls;
        rule.insert("name", "Новое имя");
        rule.insert("volume", 35);
        QVERIFY(command(engine, "setSchedule", {{"schedule", schedule({rule})}}).value("ok").toBool());
        QCOMPARE(backend.sourceCalls, sourceCalls);
        QCOMPARE(backend.playCalls, 1);
        QCOMPARE(engine.status().value("positionMs").toInteger(), 2345);
        QCOMPARE(engine.status().value("channelName").toString(), QStringLiteral("Новое имя"));
        QCOMPARE(backend.volume, 35);
        QVERIFY(command(engine, "volume", {{"value", 22}}).value("ok").toBool());
        engine.evaluateSchedule(now.addSecs(1));
        QCOMPARE(backend.volume, 22);
        rule.insert("paths", QJsonArray{second});
        QVERIFY(command(engine, "setSchedule", {{"schedule", schedule({rule})}}).value("ok").toBool());
        QCOMPARE(backend.playCalls, 2);
        QCOMPARE(backend.source.toLocalFile(), second);
        QVERIFY(command(engine, "stop").value("ok").toBool());
        QVERIFY(command(engine, "setSchedule", {{"schedule", schedule({rule})}}).value("ok").toBool());
        QCOMPARE(backend.playCalls, 2);
    }

    void scheduleConflictsAreSilentAndRecoverAtTheNextBoundary()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, "first.wav");
        const QString second = createTrack(directory, "second.wav");
        QDateTime now = at("09:59:59");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        QVERIFY(command(engine, "schedule", {{"schedule", schedule({channel("first", "Первый", "09:00", "11:00", {first}), channel("second", "Второй", "10:00", "12:00", {second})})}}).value("ok").toBool());
        QCOMPARE(backend.source.toLocalFile(), first);
        now = at("10:00:00");
        engine.evaluateSchedule(now);
        QCOMPARE(state(engine), QStringLiteral("stopped"));
        QVERIFY(engine.status().value("queue").toArray().isEmpty());
        QVERIFY(engine.status().value("scheduleError").toString().contains(QStringLiteral("Пересечение")));
        QCOMPARE(backend.playCalls, 1);
        now = at("11:00:00");
        engine.evaluateSchedule(now);
        QCOMPARE(backend.source.toLocalFile(), second);
        QCOMPARE(backend.playCalls, 2);
        QVERIFY(engine.status().value("scheduleError").toString().isEmpty());
    }

    void scheduleCalendarAndUnsupportedWindowsUseSharedSemantics()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString track = createTrack(directory, "track.wav");
        QDateTime now = at("10:00:00"); // Sunday=0 in the persisted format.
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        auto rule = channel("first", "Первый", "09:00", "12:00", {track});
        rule.insert("weekdays", "1");
        QVERIFY(command(engine, "schedule", {{"schedule", schedule({rule})}}).value("ok").toBool());
        QCOMPARE(backend.playCalls, 0);
        rule.insert("weekdays", "0");
        rule.insert("days", "4");
        rule.insert("months", "10");
        QVERIFY(command(engine, "setSchedule", {{"schedule", schedule({rule})}}).value("ok").toBool());
        QCOMPARE(backend.playCalls, 1);
        rule.insert("months", "11");
        QVERIFY(command(engine, "setSchedule", {{"schedule", schedule({rule})}}).value("ok").toBool());
        QCOMPARE(state(engine), QStringLiteral("stopped"));
        rule.insert("months", "10");
        rule.insert("start", "23:00");
        rule.insert("end", "08:00");
        QVERIFY(command(engine, "setSchedule", {{"schedule", schedule({rule})}}).value("ok").toBool());
        QVERIFY(engine.status().value("scheduleError").toString().contains(QStringLiteral("полночь")));
        QCOMPARE(backend.playCalls, 1);
    }

    void advertsRunOncePerMinuteAndReturnToCurrentChannel()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, "first.wav");
        const QString second = createTrack(directory, "second.wav");
        const QString ad = createTrack(directory, "advert.wav");
        QDateTime now = at("09:59:00");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        const auto snapshot = schedule({channel("first", "Утро", "09:00", "10:00", {first}), channel("second", "День", "10:00", "12:00", {second})}, {advert("ad", "Объявление", "59m", {ad})});
        QVERIFY(command(engine, "schedule", {{"schedule", snapshot}}).value("ok").toBool());
        QCOMPARE(backend.source.toLocalFile(), ad);
        QCOMPARE(backend.volume, 60);
        QCOMPARE(engine.status().value("repeat").toString(), QStringLiteral("off"));
        engine.evaluateSchedule(now.addSecs(20));
        QCOMPARE(backend.playCalls, 1);
        now = at("10:00:00");
        engine.evaluateSchedule(now);
        QCOMPARE(backend.source.toLocalFile(), ad);
        backend.finish();
        settle();
        QCOMPARE(backend.source.toLocalFile(), second);
        QCOMPARE(backend.volume, 75);
        QCOMPARE(engine.status().value("channelName").toString(), QStringLiteral("День"));
        QCOMPARE(engine.status().value("repeat").toString(), QStringLiteral("all"));
        QCOMPARE(backend.playCalls, 2);
        now = at("10:59:00");
        engine.evaluateSchedule(now);
        QCOMPARE(backend.source.toLocalFile(), ad);
        backend.finish();
        settle();
        QCOMPARE(backend.source.toLocalFile(), second);
        const int plays = backend.playCalls;
        engine.evaluateSchedule(now.addSecs(30));
        QCOMPARE(backend.playCalls, plays);
    }

    void advertsUseStableIdsPreparedMinutesAndCompleteAllMatchingBlocks()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString music = createTrack(directory, "music.wav");
        const QString first = createTrack(directory, "first-ad.wav");
        const QString second = createTrack(directory, "second-ad.wav");
        QDateTime now = at("10:05:00");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        auto frequency = advert("frequency", "Одинаковое имя", "2", {first}, 40);
        frequency.insert("compiledMinutes", QJsonArray{5, 35});
        const auto snapshot = schedule({channel("music", "Музыка", "09:00", "12:00", {music})},
            {frequency, advert("exact", "Одинаковое имя", "05m", {second}, 80), advert("disabled", "Выключено", "*", {second})});
        QVERIFY(command(engine, "schedule", {{"schedule", snapshot}}).value("ok").toBool());
        QCOMPARE(backend.source.toLocalFile(), first);
        QCOMPARE(backend.volume, 40);
        backend.finish();
        settle();
        QCOMPARE(backend.source.toLocalFile(), second);
        QCOMPARE(backend.volume, 80);
        backend.finish();
        settle();
        QCOMPARE(backend.source.toLocalFile(), music);
        QCOMPARE(backend.playCalls, 3);
        QVERIFY(command(engine, "setSchedule", {{"schedule", snapshot}}).value("ok").toBool());
        QCOMPARE(backend.playCalls, 3);
        now = at("10:35:00");
        engine.evaluateSchedule(now);
        QCOMPARE(backend.source.toLocalFile(), first);
        backend.fail();
        settle();
        QCOMPARE(backend.source.toLocalFile(), music);
        QCOMPARE(backend.playCalls, 5);
    }

    void advertRestoresInterruptedTrackAndPositionWhenChannelRemainsActive()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, "first.wav");
        const QString second = createTrack(directory, "second.wav");
        const QString ad = createTrack(directory, "advert.wav");
        QDateTime now = at("10:29:00");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        QVERIFY(command(engine, "schedule", {{"schedule", schedule({channel("music", "Музыка", "09:00", "12:00", {first, second})}, {advert("ad", "Реклама", "30m", {ad})})}}).value("ok").toBool());
        backend.confirmPlaying();
        backend.finish();
        settle();
        QCOMPARE(backend.source.toLocalFile(), second);
        backend.confirmPlaying();
        backend.reportPosition(4321);
        now = at("10:30:00");
        engine.evaluateSchedule(now);
        QCOMPARE(backend.source.toLocalFile(), ad);
        backend.finish();
        settle();
        QCOMPARE(backend.source.toLocalFile(), second);
        QCOMPARE(engine.status().value("currentIndex").toInt(), 1);
        QCOMPARE(engine.status().value("positionMs").toInteger(), 4321);
        QCOMPARE(backend.position, 4321);
        QCOMPARE(engine.status().value("channelName").toString(), QStringLiteral("Музыка"));
        const int plays = backend.playCalls;
        engine.evaluateSchedule(now.addSecs(20));
        QCOMPARE(backend.playCalls, plays);
    }

    void invalidScheduleSnapshotsPreserveTheActiveQueueAndMode()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString track = createTrack(directory, "track.wav");
        QDateTime now = at("10:00:00");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        const auto validChannel = channel("music", "Музыка", "09:00", "12:00", {track});
        QVERIFY(command(engine, "schedule", {{"schedule", schedule({validChannel})}}).value("ok").toBool());
        const QJsonObject before = engine.status();
        const int plays = backend.playCalls;
        auto malformedChannel = validChannel;
        malformedChannel.insert("start", "9:00");
        auto missingFile = validChannel;
        missingFile.insert("paths", QJsonArray{directory.filePath("missing.wav")});
        auto unknownField = validChannel;
        unknownField.insert("typo", true);
        auto badAdvert = advert("ad", "Реклама", "2", {track});
        badAdvert.insert("compiledMinutes", QJsonArray{5, 34});
        auto badDate = advert("ad", "Реклама", "00m", {track});
        badDate.insert("from", "2026-10-05");
        const QList<QJsonObject> invalid = {
            schedule({malformedChannel}), schedule({missingFile}), schedule({unknownField}),
            schedule({validChannel, validChannel}), schedule({validChannel}, {badAdvert}),
            schedule({validChannel}, {badDate}), {{"channels", QJsonArray{}}}
        };
        for (const auto &snapshot : invalid) {
            QVERIFY(!command(engine, "setSchedule", {{"schedule", snapshot}}).value("ok").toBool());
            QCOMPARE(engine.status(), before);
            QCOMPARE(backend.playCalls, plays);
        }
    }

    void manualControlCancelsAdvertsAndStaleScheduledContinuation_data()
    {
        QTest::addColumn<QString>("interruption");
        for (const QString &name : {QStringLiteral("stop"), QStringLiteral("pause"), QStringLiteral("clear"), QStringLiteral("load"), QStringLiteral("enqueue")})
            QTest::newRow(name.toUtf8().constData()) << name;
    }

    void manualControlCancelsAdvertsAndStaleScheduledContinuation()
    {
        QFETCH(QString, interruption);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString music = createTrack(directory, "music.wav");
        const QString ad = createTrack(directory, "advert.wav");
        QDateTime now = at("10:00:00");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        QVERIFY(command(engine, "schedule", {{"schedule", schedule({channel("music", "Музыка", "09:00", "12:00", {music})}, {advert("ad", "Реклама", "00m", {ad})})}}).value("ok").toBool());
        backend.finish();
        QJsonObject arguments;
        if (interruption == QStringLiteral("load") || interruption == QStringLiteral("enqueue"))
            arguments.insert("paths", QJsonArray{music});
        QVERIFY(command(engine, interruption, arguments).value("ok").toBool());
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("manual"));
        const int plays = backend.playCalls;
        settle();
        // Enqueue continues its now-manual queue naturally, but never restores
        // the scheduled music queue or a pending advertising block.
        if (interruption == QStringLiteral("enqueue"))
            QCOMPARE(backend.playCalls, plays + 1);
        else
            QCOMPARE(backend.playCalls, plays);
        const int settledPlays = backend.playCalls;
        engine.evaluateSchedule(at("11:00:00"));
        QCOMPARE(backend.playCalls, settledPlays);
    }

    void playbackAvailabilitySuspendsScheduleAndReevaluatesOnReconnect()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, "first.wav");
        const QString second = createTrack(directory, "second.wav");
        QDateTime now = at("09:30:00");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        QVERIFY(command(engine, "schedule", {{"schedule", schedule({channel("first", "Утро", "09:00", "10:00", {first}), channel("second", "День", "10:00", "12:00", {second})})}}).value("ok").toBool());
        QCOMPARE(backend.playCalls, 1);
        engine.setPlaybackAvailable(false);
        QCOMPARE(state(engine), QStringLiteral("stopped"));
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("schedule"));
        now = at("10:30:00");
        engine.evaluateSchedule(now);
        QCOMPARE(backend.playCalls, 1);
        engine.setPlaybackAvailable(true);
        QCOMPARE(backend.source.toLocalFile(), second);
        QCOMPARE(backend.playCalls, 2);
        QVERIFY(command(engine, "playChannel", {{"name", "Ручной"}, {"paths", QJsonArray{first}}, {"volume", 70}}).value("ok").toBool());
        engine.setPlaybackAvailable(false);
        engine.setPlaybackAvailable(true);
        QCOMPARE(backend.playCalls, 3);
        QCOMPARE(state(engine), QStringLiteral("stopped"));
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("manual"));
    }

    void emptyScheduledChannelIsDiagnosedWithoutAPlaybackLoop()
    {
        QDateTime now = at("10:00:00");
        FakeAudioBackend backend;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        QVERIFY(command(engine, "schedule", {{"schedule", schedule({channel("empty", "Пустой канал", "09:00", "12:00", {})})}}).value("ok").toBool());
        QCOMPARE(backend.playCalls, 0);
        QVERIFY(engine.status().value("scheduleError").toString().contains(QStringLiteral("не содержит файлов")));
        const int sourceCalls = backend.sourceCalls;
        engine.evaluateSchedule(now.addSecs(1));
        QCOMPARE(backend.sourceCalls, sourceCalls);
    }

    void failedScheduledChannelDoesNotRetryEveryTimerTick()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, "first.wav");
        const QString second = createTrack(directory, "second.wav");
        QDateTime now = at("10:00:00");
        FakeAudioBackend backend;
        backend.rejectEveryTrack = true;
        PlayerEngine engine(&backend, nullptr, [&now] { return now; });
        QVERIFY(command(engine, "schedule", {{"schedule", schedule({channel("broken", "Недоступно", "09:00", "12:00", {first, second})})}}).value("ok").toBool());
        settle();
        settle();
        QCOMPARE(state(engine), QStringLiteral("error"));
        QCOMPARE(backend.playCalls, 2);
        for (int second = 1; second <= 10; ++second)
            engine.evaluateSchedule(now.addSecs(second));
        settle();
        QCOMPARE(backend.playCalls, 2);
        QCOMPARE(state(engine), QStringLiteral("error"));
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("schedule"));
    }

    void queueValidationIsAtomic()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, QStringLiteral("Первый трек.wav"));
        const QString second = createTrack(directory, QStringLiteral("second.wav"));
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        QVERIFY(load(engine, {first, second}, true, 1).value("ok").toBool());
        backend.confirmPlaying();
        const QJsonObject before = engine.status();
        const int playsBefore = backend.playCalls;
        QJsonArray oversizedQueue;
        for (int index = 0; index < 1001; ++index)
            oversizedQueue.append(first);

        const QList<QJsonObject> invalidLoads = {
            {{"paths", QJsonArray{}}},
            {{"paths", oversizedQueue}},
            {{"paths", QJsonArray{first, directory.filePath("missing.wav")}}},
            {{"paths", QJsonArray{QStringLiteral("relative.wav")}}},
            {{"paths", QJsonArray{directory.path()}}},
            {{"paths", QJsonArray{first, 42}}},
            {{"paths", first}},
            {{"paths", QJsonArray{first}}, {"startIndex", 1}},
            {{"paths", QJsonArray{first}}, {"startIndex", -1}},
            {{"paths", QJsonArray{first}}, {"startIndex", 0.5}},
            {{"paths", QJsonArray{first}}, {"autoplay", "yes"}}
        };
        for (const QJsonObject &arguments : invalidLoads) {
            const QJsonObject response = command(engine, "load", arguments);
            QVERIFY2(!response.value("ok").toBool(), qPrintable(QString::fromUtf8(
                QJsonDocument(arguments).toJson(QJsonDocument::Compact))));
            QVERIFY(response.value("error").isObject());
            QCOMPARE(engine.status(), before);
            QCOMPARE(backend.playCalls, playsBefore);
        }

        const QJsonObject response = command(engine, "enqueue", {
            {"paths", QJsonArray{second, directory.filePath("missing.wav")}}
        });
        QVERIFY(!response.value("ok").toBool());
        QCOMPARE(engine.status(), before);
    }

    void backendConfirmsPlaybackState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString track = createTrack(directory, QStringLiteral("track.wav"));
        QVERIFY(!track.isEmpty());
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        QSignalSpy changed(&engine, &PlayerEngine::statusChanged);

        QVERIFY(load(engine, {track}).value("ok").toBool());
        QCOMPARE(state(engine), QStringLiteral("stopped"));
        QVERIFY(command(engine, "play").value("ok").toBool());
        QCOMPARE(state(engine), QStringLiteral("loading"));
        QCOMPARE(backend.playCalls, 1);
        backend.confirmPlaying();
        QCOMPARE(state(engine), QStringLiteral("playing"));
        backend.reportDuration(9000);
        backend.reportPosition(1200);
        QCOMPARE(engine.status().value("durationMs").toInteger(), 9000);
        QCOMPARE(engine.status().value("positionMs").toInteger(), 1200);

        QVERIFY(command(engine, "pause").value("ok").toBool());
        QCOMPARE(backend.pauseCalls, 1);
        backend.confirmPaused();
        QCOMPARE(state(engine), QStringLiteral("paused"));
        QVERIFY(command(engine, "play").value("ok").toBool());
        backend.confirmPlaying();
        QCOMPARE(state(engine), QStringLiteral("playing"));
        QVERIFY(!changed.isEmpty());
    }

    void naturalEndRespectsRepeatMode_data()
    {
        QTest::addColumn<QString>("repeatMode");
        QTest::addColumn<int>("nextIndex");
        QTest::addColumn<int>("expectedPlayCalls");
        QTest::newRow("off") << QStringLiteral("off") << 1 << 1;
        QTest::newRow("all") << QStringLiteral("all") << 0 << 2;
        QTest::newRow("one") << QStringLiteral("one") << 1 << 2;
    }

    void naturalEndRespectsRepeatMode()
    {
        QFETCH(QString, repeatMode);
        QFETCH(int, nextIndex);
        QFETCH(int, expectedPlayCalls);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, QStringLiteral("first.wav"));
        const QString second = createTrack(directory, QStringLiteral("second.wav"));
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        QVERIFY(command(engine, "repeat", {{"mode", repeatMode}}).value("ok").toBool());
        QVERIFY(load(engine, {first, second}, true, 1).value("ok").toBool());
        backend.confirmPlaying();
        backend.finish();
        settle();
        QCOMPARE(engine.status().value("currentIndex").toInt(), nextIndex);
        QCOMPARE(backend.playCalls, expectedPlayCalls);
        if (repeatMode == QStringLiteral("off"))
            QCOMPARE(state(engine), QStringLiteral("stopped"));
        else
            QCOMPARE(backend.source.toLocalFile(), nextIndex == 0 ? first : second);
    }

    void naturalEndAdvancesQueue()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, QStringLiteral("first.wav"));
        const QString second = createTrack(directory, QStringLiteral("second.wav"));
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        QVERIFY(load(engine, {first, second}, true).value("ok").toBool());
        backend.confirmPlaying();
        backend.finish();
        QTRY_COMPARE(backend.playCalls, 2);
        QCOMPARE(engine.status().value("currentIndex").toInt(), 1);
        QCOMPARE(backend.source.toLocalFile(), second);
    }

    void failedTrackIsSkipped()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, QStringLiteral("broken.wav"));
        const QString second = createTrack(directory, QStringLiteral("good.wav"));
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        QVERIFY(load(engine, {first, second}, true).value("ok").toBool());
        backend.fail();
        QTRY_COMPARE(backend.playCalls, 2);
        QCOMPARE(backend.source.toLocalFile(), second);
        backend.confirmPlaying();
        QCOMPARE(state(engine), QStringLiteral("playing"));
    }

    void errorsStopAfterOneCycle_data()
    {
        QTest::addColumn<QString>("repeatMode");
        QTest::newRow("off") << QStringLiteral("off");
        QTest::newRow("all") << QStringLiteral("all");
        QTest::newRow("one") << QStringLiteral("one");
    }

    void errorsStopAfterOneCycle()
    {
        QFETCH(QString, repeatMode);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, QStringLiteral("broken1.wav"));
        const QString second = createTrack(directory, QStringLiteral("broken2.wav"));
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        FakeAudioBackend backend;
        backend.rejectEveryTrack = true;
        PlayerEngine engine(&backend);
        QVERIFY(command(engine, "repeat", {{"mode", repeatMode}}).value("ok").toBool());
        QVERIFY(load(engine, {first, second}, true).value("ok").toBool());
        QTRY_COMPARE(backend.playCalls, 2);
        QTRY_COMPARE(state(engine), QStringLiteral("error"));
        const int playCalls = backend.playCalls;
        QVERIFY(playCalls <= 2);
        QVERIFY(!engine.status().value("error").toString().isEmpty());
        settle();
        QCOMPARE(backend.playCalls, playCalls);
    }

    void missingFileIsSkippedAfterQueueWasLoaded()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, QStringLiteral("first.wav"));
        const QString missing = createTrack(directory, QStringLiteral("removed.wav"));
        const QString last = createTrack(directory, QStringLiteral("last.wav"));
        QVERIFY(!first.isEmpty());
        QVERIFY(!missing.isEmpty());
        QVERIFY(!last.isEmpty());
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        QVERIFY(load(engine, {first, missing, last}, true).value("ok").toBool());
        backend.confirmPlaying();
        QVERIFY(QFile::remove(missing));
        backend.finish();
        QTRY_COMPARE(engine.status().value("currentIndex").toInt(), 2);
        QCOMPARE(backend.playedSources, (QStringList{first, last}));
    }

    void retryAfterDecoderFailureReloadsSource()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString track = createTrack(directory, QStringLiteral("track.wav"));
        QVERIFY(!track.isEmpty());
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        QVERIFY(load(engine, {track}, true).value("ok").toBool());
        backend.fail();
        QVERIFY(command(engine, "stop").value("ok").toBool());
        const int sourcesBeforeRetry = backend.sourceCalls;
        QVERIFY(command(engine, "play").value("ok").toBool());
        QCOMPARE(backend.sourceCalls, sourcesBeforeRetry + 1);
        backend.confirmPlaying();
        QCOMPARE(state(engine), QStringLiteral("playing"));
        settle();
        QCOMPARE(backend.playCalls, 2);
        QCOMPARE(state(engine), QStringLiteral("playing"));
    }

    void staleTransitionsDoNotRestartPlayback_data()
    {
        QTest::addColumn<QString>("transition");
        QTest::addColumn<QString>("interruption");
        for (const QString &transition : {QStringLiteral("finish"), QStringLiteral("error")}) {
            for (const QString &interruption : {QStringLiteral("stop"), QStringLiteral("clear"),
                                                QStringLiteral("load"), QStringLiteral("pause"),
                                                QStringLiteral("next"), QStringLiteral("previous")}) {
                const QByteArray name = (transition + '-' + interruption).toUtf8();
                QTest::newRow(name.constData()) << transition << interruption;
            }
        }
    }

    void staleTransitionsDoNotRestartPlayback()
    {
        QFETCH(QString, transition);
        QFETCH(QString, interruption);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, QStringLiteral("first.wav"));
        const QString second = createTrack(directory, QStringLiteral("second.wav"));
        const QString replacement = createTrack(directory, QStringLiteral("replacement.wav"));
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        QVERIFY(!replacement.isEmpty());
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        QVERIFY(load(engine, {first, second}, true).value("ok").toBool());
        backend.confirmPlaying();
        if (transition == QStringLiteral("finish"))
            backend.finish();
        else
            backend.fail();

        const QJsonObject response = interruption == QStringLiteral("load")
            ? load(engine, {replacement}) : command(engine, interruption);
        QVERIFY(response.value("ok").toBool());
        const int playCalls = backend.playCalls;
        const QJsonObject expectedStatus = engine.status();
        settle();
        QCOMPARE(backend.playCalls, playCalls);
        QCOMPARE(engine.status(), expectedStatus);
        if (interruption == QStringLiteral("next") || interruption == QStringLiteral("previous"))
            QCOMPARE(playCalls, 2);
        else
            QCOMPARE(playCalls, 1);
        if (interruption == QStringLiteral("clear")) {
            QVERIFY(engine.status().value("queue").toArray().isEmpty());
            QCOMPARE(engine.status().value("currentIndex").toInt(), -1);
        } else if (interruption == QStringLiteral("load")) {
            QCOMPARE(engine.status().value("queue").toArray(), QJsonArray{replacement});
            QCOMPARE(engine.status().value("currentTrack").toString(), replacement);
        }
    }

    void stopRewinds()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString track = createTrack(directory, QStringLiteral("track.wav"));
        QVERIFY(!track.isEmpty());
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        QVERIFY(load(engine, {track}, true).value("ok").toBool());
        backend.confirmPlaying();
        backend.reportDuration(10000);
        backend.reportPosition(5000);
        QVERIFY(command(engine, "stop").value("ok").toBool());
        QCOMPARE(state(engine), QStringLiteral("stopped"));
        QCOMPARE(engine.status().value("positionMs").toInteger(), 0);
        QCOMPARE(backend.position, 0);
        QVERIFY(command(engine, "play").value("ok").toBool());
        QCOMPARE(backend.position, 0);
    }

    void seekVolumeAndMuteValidate()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString track = createTrack(directory, QStringLiteral("track.wav"));
        QVERIFY(!track.isEmpty());
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        QVERIFY(load(engine, {track}, true).value("ok").toBool());
        backend.confirmPlaying();
        backend.reportDuration(10000);
        QVERIFY(command(engine, "seek", {{"positionMs", 4500}}).value("ok").toBool());
        QCOMPARE(backend.position, 4500);
        for (const QJsonValue &value : {QJsonValue(-1), QJsonValue(10001), QJsonValue(0.5),
                                      QJsonValue(QStringLiteral("1000"))}) {
            QVERIFY(!command(engine, "seek", {{"positionMs", value}}).value("ok").toBool());
            QCOMPARE(backend.position, 4500);
        }

        QVERIFY(command(engine, "volume", {{"value", 37}}).value("ok").toBool());
        QCOMPARE(backend.volume, 37);
        QCOMPARE(engine.status().value("volumePercent").toInt(), 37);
        const QJsonObject beforeUnknownField = engine.status();
        QVERIFY(!command(engine, "volume", {{"value", 50}, {"typo", true}}).value("ok").toBool());
        QCOMPARE(engine.status(), beforeUnknownField);
        QCOMPARE(backend.volume, 37);
        for (const QJsonValue &value : {QJsonValue(-1), QJsonValue(101), QJsonValue(0.5),
                                      QJsonValue(QStringLiteral("50"))}) {
            QVERIFY(!command(engine, "volume", {{"value", value}}).value("ok").toBool());
            QCOMPARE(backend.volume, 37);
        }
        QVERIFY(command(engine, "mute", {{"value", true}}).value("ok").toBool());
        QVERIFY(backend.muted);
        QVERIFY(engine.status().value("muted").toBool());
        QVERIFY(!command(engine, "mute", {{"value", 1}}).value("ok").toBool());
        QVERIFY(backend.muted);
        QVERIFY(command(engine, "mute", {{"value", false}}).value("ok").toBool());
        QVERIFY(!backend.muted);
        QVERIFY(!command(engine, "repeat", {{"mode", "random"}}).value("ok").toBool());
    }

    void queueControls()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = createTrack(directory, QStringLiteral("first.wav"));
        const QString second = createTrack(directory, QStringLiteral("second.wav"));
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        QVERIFY(!command(engine, "play").value("ok").toBool());
        QVERIFY(load(engine, {first}, true).value("ok").toBool());
        backend.confirmPlaying();
        QVERIFY(command(engine, "enqueue", {{"paths", QJsonArray{second}}}).value("ok").toBool());
        QCOMPARE(engine.status().value("queue").toArray(), (QJsonArray{first, second}));
        QCOMPARE(backend.playCalls, 1);
        QVERIFY(command(engine, "next").value("ok").toBool());
        QCOMPARE(engine.status().value("currentIndex").toInt(), 1);
        QCOMPARE(backend.source.toLocalFile(), second);
        backend.confirmPlaying();
        QVERIFY(command(engine, "previous").value("ok").toBool());
        QCOMPARE(engine.status().value("currentIndex").toInt(), 0);
        QCOMPARE(backend.source.toLocalFile(), first);
        QVERIFY(command(engine, "clear").value("ok").toBool());
        QCOMPARE(state(engine), QStringLiteral("stopped"));
        QCOMPARE(engine.status().value("currentIndex").toInt(), -1);
        QVERIFY(engine.status().value("queue").toArray().isEmpty());
        QVERIFY(!command(engine, "play").value("ok").toBool());
    }

    void invalidCommandsLeaveStateUntouched()
    {
        FakeAudioBackend backend;
        PlayerEngine engine(&backend);
        const QJsonObject before = engine.status();
        for (const QJsonObject &request : {QJsonObject{}, QJsonObject{{"command", 1}},
                                         QJsonObject{{"command", "unknown"}}}) {
            const QJsonObject response = engine.execute(request);
            QVERIFY(!response.value("ok").toBool());
            QVERIFY(!response.value("error").toObject().value("code").toString().isEmpty());
            QCOMPARE(engine.status(), before);
        }
        const QJsonObject response = command(engine, "status");
        QVERIFY(response.value("ok").toBool());
        QCOMPARE(response.value("status").toObject(), before);
    }
};

QTEST_GUILESS_MAIN(PlayerEngineTest)
#include "tst_playerengine.moc"
