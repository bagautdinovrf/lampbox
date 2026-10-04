#include "playerengine.h"
#include "schedulev1runtime.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace MediaBox;

namespace {
QString id(int n) { return QStringLiteral("00000000-0000-4000-8000-%1").arg(n, 12, 10, QLatin1Char('0')); }
QJsonObject allDays() { return {{"select", QJsonObject{{"type", "all"}}}, {"excludeDates", QJsonArray{}}}; }
QJsonObject source(int n) { return {{"type", "playlist"}, {"playlistId", id(n)}}; }
QByteArray bytes(const QJsonObject &value) { return QJsonDocument(value).toJson(QJsonDocument::Compact); }
QDateTime at(const char *value) { return QDateTime::fromString(QString::fromLatin1(value), Qt::ISODate); }
QDateTime testClock() { return at("2026-12-16T11:00:00Z"); }
QJsonObject schedule()
{
    QJsonArray assets;
    for (int n = 1; n <= 5; ++n)
        assets.append(QJsonObject{{"id", id(100 + n)}, {"path", QStringLiteral("%1.mp3").arg(n)}, {"mediaType", "audio"}});
    const auto playlist = [](int n, int first) {
        return QJsonObject{{"id", id(n)}, {"revision", 1}, {"name", QStringLiteral("Список %1").arg(n)},
            {"order", "sequential"}, {"entries", QJsonArray{
                QJsonObject{{"id", id(first + 100)}, {"assetId", id(first)}},
                QJsonObject{{"id", id(first + 101)}, {"assetId", id(first + 1)}}}}};
    };
    const QJsonObject window{{"from", "00:00:00"}, {"until", "00:00:00"}, {"untilDayOffset", 1}};
    return {{"format", "mediabox.schedule"}, {"schemaVersion", 1}, {"scheduleId", id(1)}, {"stationId", id(2)},
        {"publicationId", id(3)}, {"revision", 1}, {"publishedAt", "2026-10-04T09:00:00Z"}, {"timeZone", "UTC"},
        {"validity", QJsonObject{{"from", "2026-12-15"}, {"until", "2027-01-16"}}},
        {"requiredCapabilities", QJsonArray{"calendar.v1", "rotation.strict.v1"}},
        {"musicTransition", "finish_track"}, {"timeResolution", QJsonObject{{"gap", "skip"}, {"overlap", "first"}}},
        {"fallback", QJsonObject{{"source", QJsonObject{{"type", "silence"}}}, {"volumePercent", 0}}},
        {"assets", assets}, {"playlists", QJsonArray{playlist(301, 101), playlist(302, 103)}},
        {"calendars", QJsonArray{}}, {"dayTemplates", QJsonArray{QJsonObject{{"id", id(401)}, {"name", "Сутки"},
            {"slots", QJsonArray{QJsonObject{{"id", id(411)}, {"window", window}, {"source", source(301)}, {"volumePercent", 70}}}}}}},
        {"baseRules", QJsonArray{QJsonObject{{"id", id(501)}, {"name", "Каждый день"}, {"enabled", true},
            {"priority", 1}, {"when", allDays()}, {"templateId", id(401)}}}},
        {"mixRules", QJsonArray{QJsonObject{{"id", id(601)}, {"name", "Через один"}, {"enabled", true},
            {"priority", 1}, {"when", allDays()}, {"windows", QJsonArray{window}},
            {"pattern", QJsonArray{QJsonObject{{"type", "active_base"}}, source(302)}}, {"emptyAdditionalSource", "use_base"}}}},
        {"eventRules", QJsonArray{}}};
}

void addEvent(QJsonObject &document, const QString &delivery, int late = 60, int number = 701, int priority = 10)
{
    QJsonArray events = document.value("eventRules").toArray();
    events.append(QJsonObject{{"id", id(number)}, {"name", QStringLiteral("Объявление %1").arg(number)}, {"enabled", true},
        {"priority", priority}, {"when", allDays()}, {"times", QJsonArray{"12:00:00"}},
        {"action", QJsonObject{{"assetId", id(105)}, {"volumePercent", 80}}},
        {"delivery", QJsonObject{{"start", delivery}, {"maxLateSeconds", late}, {"expired", "skip"}, {"after", "resume_music"}}}});
    document.insert("eventRules", events);
    document.insert("requiredCapabilities", QJsonArray{"calendar.v1", "rotation.strict.v1", "events.fixed.v1"});
}

void files(const QTemporaryDir &directory)
{
    for (int n = 1; n <= 5; ++n) {
        QFile file(directory.filePath(QStringLiteral("%1.mp3").arg(n)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("test audio");
    }
}

QJsonObject active(const QJsonObject &document, const QByteArray &data)
{
    return {{"format", "mediabox.active"}, {"schemaVersion", 1}, {"scheduleId", document.value("scheduleId")},
        {"stationId", document.value("stationId")}, {"publicationId", document.value("publicationId")},
        {"revision", document.value("revision")}, {"snapshotPath", QStringLiteral("snapshots/%1.json").arg(document.value("publicationId").toString())},
        {"sha256", QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex())}};
}

QString publishFiles(const QString &root, const QJsonObject &document, bool corruptHash = false)
{
    const QByteArray data = bytes(document);
    const QJsonObject pointer = active(document, data);
    QDir().mkpath(QDir(root).filePath(QStringLiteral("snapshots")));
    const auto write = [](const QString &path, const QByteArray &contents) {
        QSaveFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size() && file.commit();
    };
    if (!write(QDir(root).filePath(pointer.value("snapshotPath").toString()), data))
        return {};
    QJsonObject savedPointer = pointer;
    if (corruptHash)
        savedPointer.insert("sha256", QString(64, u'0'));
    const QString path = QDir(root).filePath(QStringLiteral("active.json"));
    return write(path, bytes(savedPointer)) ? path : QString();
}

QJsonObject publication(PlayerEngine &engine, const QJsonObject &document,
                        const QTemporaryDir &directory, bool autoplay = true)
{
    const QString path = publishFiles(directory.path(), document);
    return engine.execute({{"command", "loadPublication"}, {"activePath", path},
        {"contentRoot", directory.path()}, {"autoplay", autoplay}});
}

QString executeSql(const QString &path, const QString &statement, const QVariantList &values = {})
{
    QString error;
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", "engine-storage-failure-test");
        db.setDatabaseName(path);
        if (!db.open()) {
            error = db.lastError().text();
        } else {
            QSqlQuery query(db);
            query.prepare(statement);
            for (const auto &value : values)
                query.addBindValue(value);
            if (!query.exec())
                error = query.lastError().text();
        }
        db.close();
    }
    QSqlDatabase::removeDatabase("engine-storage-failure-test");
    return error;
}

QVariant storedValue(const QString &path, const QString &statement)
{
    QVariant result;
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", "engine-storage-inspection-test");
        db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(statement) && query.next())
                result = query.value(0);
        }
        db.close();
    }
    QSqlDatabase::removeDatabase("engine-storage-inspection-test");
    return result;
}

QString addRetentionRows(const QString &path, const QString &label, const QDateTime &timestamp)
{
    const QString key = QStringLiteral("retention-") + label;
    const QString stamp = timestamp.toUTC().toString(Qt::ISODateWithMs);
    const QList<QPair<QString, QVariantList>> inserts{
        {QStringLiteral("INSERT INTO starts(playback_id,schedule_id,publication_id,entry_id,at) VALUES(?,?,?,?,?)"),
            {key, key, key, QStringLiteral("entry"), stamp}},
        {QStringLiteral("INSERT INTO diagnostics(at,publication_id,playback_id,message) VALUES(?,?,?,?)"),
            {stamp, key, key, QStringLiteral("retention test")}},
        {QStringLiteral("INSERT INTO events(event_key,schedule_id,rule_id,scheduled_utc,state) VALUES(?,?,?,?,?)"),
            {key, key, QStringLiteral("rule"), stamp, QStringLiteral("completed")}},
        {QStringLiteral("INSERT INTO publications(publication_id,schedule_id,station_id,revision,sha256,last_used_at) VALUES(?,?,?,?,?,?)"),
            {key, key, id(2), 1, QStringLiteral("digest"), stamp}},
        {QStringLiteral("INSERT INTO runtime_state(schedule_id,state,updated_at) VALUES(?,?,?)"),
            {key, QByteArray("{}"), stamp}}
    };
    for (const auto &insert : inserts) {
        const QString error = executeSql(path, insert.first, insert.second);
        if (!error.isEmpty())
            return error;
    }
    return {};
}

QList<int> retentionRowCounts(const QString &path, const QString &label)
{
    const QList<QPair<QString, QString>> keys{{"starts", "playback_id"}, {"diagnostics", "playback_id"},
        {"events", "event_key"}, {"publications", "publication_id"}, {"runtime_state", "schedule_id"}};
    QList<int> result;
    for (const auto &key : keys) {
        const auto count = storedValue(path, QStringLiteral("SELECT COUNT(*) FROM %1 WHERE %2='retention-%3'")
            .arg(key.first, key.second, label));
        result.append(count.isValid() ? count.toInt() : -1);
    }
    return result;
}

class Backend final : public AudioBackend
{
public:
    void setSource(const QUrl &value) override { path = value.toLocalFile(); ++loads; }
    void play() override { ++plays; }
    void pause() override { emit stateChanged(State::Paused); }
    void stop() override { emit stateChanged(State::Stopped); }
    void seek(qint64 position) override { seekPosition = position; emit positionChanged(position); }
    void setVolume(int value) override { volume = value; }
    void setMuted(bool) override {}
    void playing() { emit stateChanged(State::Playing); }
    void finish() { emit stateChanged(State::Stopped); emit finished(); }
    void fail() { emit errorOccurred(QStringLiteral("decode failure")); }
    QString path;
    int loads = 0, plays = 0, volume = 0;
    qint64 seekPosition = 0;
};
}

class ScheduleV1RuntimeTest : public QObject
{
    Q_OBJECT
private slots:
    void historyRetentionHonorsThirtyDayBoundary_data()
    {
        QTest::addColumn<bool>("restart");
        QTest::newRow("explicit-maintenance") << false;
        QTest::newRow("startup-maintenance") << true;
    }

    void historyRetentionHonorsThirtyDayBoundary()
    {
        QFETCH(bool, restart);
        QTemporaryDir directory;
        files(directory);
        const QString database = directory.filePath("runtime.sqlite");
        auto now = at("2027-01-15T12:00:00Z");
        const auto cutoff = now.addDays(-30);
        const QList<int> present{1, 1, 1, 1, 1}, absent{0, 0, 0, 0, 0};
        {
            ScheduleV1Runtime runtime(database, [&] { return now; });
            QCOMPARE(runtime.accept(bytes(schedule()), directory.path(), {}, now), QString());
            QCOMPARE(addRetentionRows(database, "old", cutoff.addMSecs(-1)), QString());
            QCOMPARE(addRetentionRows(database, "boundary", cutoff), QString());
            QCOMPARE(addRetentionRows(database, "recent", now.addDays(-29)), QString());
            QCOMPARE(addRetentionRows(database, "future", now.addDays(1)), QString());
            if (!restart) {
                QCOMPARE(runtime.pruneHistory(), QString());
                QCOMPARE(retentionRowCounts(database, "old"), absent);
                QCOMPARE(retentionRowCounts(database, "boundary"), present);
                QCOMPARE(retentionRowCounts(database, "recent"), present);
                QCOMPARE(retentionRowCounts(database, "future"), present);
            }
        }
        ScheduleV1Runtime restored(database, [&] { return now; });
        QCOMPARE(restored.restore(), QString());
        QCOMPARE(retentionRowCounts(database, "old"), absent);
        QCOMPARE(retentionRowCounts(database, "boundary"), present);
        QCOMPARE(retentionRowCounts(database, "recent"), present);
        QCOMPARE(retentionRowCounts(database, "future"), present);
        QCOMPARE(restored.document().publicationId(), id(3));
        now = now.addMSecs(1);
        QCOMPARE(restored.pruneHistory(), QString());
        QCOMPARE(retentionRowCounts(database, "boundary"), absent);
        QCOMPARE(retentionRowCounts(database, "recent"), present);
        QCOMPARE(retentionRowCounts(database, "future"), present);
    }

    void historyRetentionPreservesActivePlayback_data()
    {
        QTest::addColumn<bool>("shuffle");
        QTest::newRow("sequential-cursor") << false;
        QTest::newRow("shuffle-remaining-cycle") << true;
    }

    void historyRetentionPreservesActivePlayback()
    {
        QFETCH(bool, shuffle);
        QTemporaryDir directory;
        files(directory);
        const QString database = directory.filePath("runtime.sqlite");
        auto now = at("2026-12-16T11:00:00Z");
        auto document = schedule();
        document.insert("validity", QJsonObject{{"from", "2026-12-15"}, {"until", "2027-03-16"}});
        if (shuffle) {
            auto playlists = document.value("playlists").toArray();
            auto base = playlists.first().toObject();
            base.insert("order", "shuffle_cycle");
            playlists.replace(0, base);
            document.insert("playlists", playlists);
        }
        QByteArray savedState, savedSnapshot;
        QString nextEntry;
        {
            ScheduleV1Runtime runtime(database, [&] { return now; });
            QCOMPARE(runtime.accept(bytes(document), directory.path(), {}, now), QString());
            QCOMPARE(runtime.setScheduledPlayback(true), QString());
            const auto first = runtime.selectMusic(now);
            QVERIFY(first.isValid());
            QCOMPARE(runtime.confirmStarted(first), QString());
            const auto additional = runtime.selectMusic(now);
            QVERIFY(additional.path.endsWith("3.mp3"));
            QCOMPARE(runtime.confirmStarted(additional), QString());
            nextEntry = runtime.selectMusic(now).entryId;
            QVERIFY(nextEntry != first.entryId);
            savedState = storedValue(database, "SELECT state FROM runtime_state").toByteArray();
            savedSnapshot = storedValue(database, "SELECT bytes FROM accepted").toByteArray();
            now = now.addDays(31);
            QCOMPARE(runtime.pruneHistory(), QString());
            QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM starts").toInt(), 0);
            QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM publications").toInt(), 1);
            QCOMPARE(storedValue(database, "SELECT state FROM runtime_state").toByteArray(), savedState);
            QCOMPARE(storedValue(database, "SELECT bytes FROM accepted").toByteArray(), savedSnapshot);
            QVERIFY(runtime.scheduledPlaybackEnabled());
            QCOMPARE(runtime.selectMusic(now).entryId, nextEntry);
        }
        ScheduleV1Runtime restored(database, [&] { return now; });
        QCOMPARE(restored.restore(), QString());
        QVERIFY(restored.scheduledPlaybackEnabled());
        QCOMPARE(restored.contentRoot(), QDir::cleanPath(directory.path()));
        QCOMPARE(restored.document().publicationId(), id(3));
        QCOMPARE(restored.selectMusic(now).entryId, nextEntry);
        QCOMPARE(storedValue(database, "SELECT state FROM runtime_state").toByteArray(), savedState);
    }

    void inactiveScheduleRetentionStartsWhenReplaced()
    {
        QTemporaryDir directory;
        files(directory);
        const QString database = directory.filePath("runtime.sqlite");
        auto now = at("2026-12-16T11:00:00Z");
        ScheduleV1Runtime runtime(database, [&] { return now; });
        QCOMPARE(runtime.accept(bytes(schedule()), directory.path(), {}, now), QString());
        QCOMPARE(runtime.confirmStarted(runtime.selectMusic(now)), QString());
        now = now.addDays(90);
        auto replacement = schedule();
        replacement.insert("scheduleId", id(901));
        replacement.insert("publicationId", id(903));
        QCOMPARE(runtime.accept(bytes(replacement), directory.path(), {}, now), QString());
        const QString retiredAt = now.toUTC().toString(Qt::ISODateWithMs);
        QCOMPARE(storedValue(database, QStringLiteral("SELECT last_used_at FROM publications WHERE publication_id='%1'").arg(id(3))).toString(), retiredAt);
        QCOMPARE(storedValue(database, QStringLiteral("SELECT updated_at FROM runtime_state WHERE schedule_id='%1'").arg(id(1))).toString(), retiredAt);
        now = now.addDays(30);
        QCOMPARE(runtime.pruneHistory(), QString());
        QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM publications").toInt(), 2);
        QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM runtime_state").toInt(), 2);
        now = now.addMSecs(1);
        QCOMPARE(runtime.pruneHistory(), QString());
        QCOMPARE(storedValue(database, "SELECT publication_id FROM publications").toString(), id(903));
        QCOMPARE(storedValue(database, "SELECT schedule_id FROM runtime_state").toString(), id(901));
    }

    void historyRetentionRollsBackAllTablesOnFailure()
    {
        QTemporaryDir directory;
        files(directory);
        const QString database = directory.filePath("runtime.sqlite");
        auto now = at("2027-01-15T12:00:00Z");
        ScheduleV1Runtime runtime(database, [&] { return now; });
        QCOMPARE(runtime.accept(bytes(schedule()), directory.path(), {}, now), QString());
        QCOMPARE(runtime.setScheduledPlayback(true), QString());
        QCOMPARE(addRetentionRows(database, "old", now.addDays(-31)), QString());
        const auto previousFloor = storedValue(database, "SELECT value FROM preferences WHERE name='retentionCutoffMs'");
        QVERIFY(previousFloor.isValid());
        QCOMPARE(executeSql(database, "CREATE TRIGGER fail_retention BEFORE DELETE ON publications "
            "BEGIN SELECT RAISE(ABORT,'injected retention failure'); END"), QString());
        now = now.addDays(1);
        QVERIFY(runtime.pruneHistory().contains("injected retention failure"));
        QVERIFY(runtime.diagnostic().contains("injected retention failure"));
        const QList<int> present{1, 1, 1, 1, 1}, absent{0, 0, 0, 0, 0};
        QCOMPARE(retentionRowCounts(database, "old"), present);
        QCOMPARE(storedValue(database, "SELECT value FROM preferences WHERE name='retentionCutoffMs'"), previousFloor);
        QVERIFY(runtime.scheduledPlaybackEnabled());
        QCOMPARE(runtime.document().publicationId(), id(3));
        QCOMPARE(executeSql(database, "DROP TRIGGER fail_retention"), QString());
        QCOMPARE(runtime.pruneHistory(), QString());
        QCOMPARE(retentionRowCounts(database, "old"), absent);
        QCOMPARE(storedValue(database, "SELECT value FROM preferences WHERE name='retentionCutoffMs'").toLongLong(), now.addDays(-30).toMSecsSinceEpoch());
        QVERIFY(!runtime.diagnostic().contains("injected retention failure"));
    }

    void removedEventCannotReplayAfterClockRollbackOrRestart()
    {
        QTemporaryDir directory;
        files(directory);
        const QString database = directory.filePath("runtime.sqlite");
        auto now = at("2026-12-16T12:00:00Z");
        const auto eventTime = now;
        qint64 retainedFloor = 0;
        ScheduleV1::EventOccurrence occurrence;
        {
            auto document = schedule();
            addEvent(document, "interrupt");
            ScheduleV1Runtime runtime(database, [&] { return now; });
            QCOMPARE(runtime.accept(bytes(document), directory.path(), {}, now), QString());
            const auto events = runtime.dueEvents(now);
            QCOMPARE(events.size(), 1);
            occurrence = events.first();
            const auto track = runtime.startEvent(occurrence, now);
            QVERIFY(track.isValid());
            QCOMPARE(runtime.confirmStarted(track), QString());
            QCOMPARE(runtime.finishEvent(track, "completed"), QString());
            now = now.addDays(31);
            QCOMPARE(runtime.pruneHistory(), QString());
            QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM events").toInt(), 0);
            QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM starts").toInt(), 0);
            retainedFloor = storedValue(database, "SELECT value FROM preferences WHERE name='retentionCutoffMs'").toLongLong();
            now = eventTime;
            QCOMPARE(runtime.pruneHistory(), QString());
            QCOMPARE(storedValue(database, "SELECT value FROM preferences WHERE name='retentionCutoffMs'").toLongLong(), retainedFloor);
            QVERIFY(runtime.dueEvents(now).isEmpty());
            QVERIFY(!runtime.startEvent(occurrence, now).isValid());
            QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM events").toInt(), 0);
        }
        ScheduleV1Runtime restarted(database, [&] { return now; });
        QCOMPARE(restarted.restore(), QString());
        QCOMPARE(storedValue(database, "SELECT value FROM preferences WHERE name='retentionCutoffMs'").toLongLong(), retainedFloor);
        QVERIFY(restarted.dueEvents(now).isEmpty());
        QVERIFY(!restarted.startEvent(occurrence, now).isValid());
        QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM events").toInt(), 0);
    }

    void removedStartDoesNotRepeatOnPlayingOrSuspendedResume()
    {
        QTemporaryDir directory;
        files(directory);
        const QString database = directory.filePath("runtime.sqlite");
        auto now = at("2026-12-16T11:59:59Z");
        auto document = schedule();
        addEvent(document, "interrupt");
        Backend backend;
        PlayerEngine engine(&backend, nullptr, [&] { return now; }, database);
        QVERIFY(publication(engine, document, directory).value("ok").toBool());
        backend.playing();
        QVERIFY(backend.path.endsWith("1.mp3"));
        const auto savedState = storedValue(database, "SELECT state FROM runtime_state").toByteArray();
        QCOMPARE(executeSql(database, "DELETE FROM starts"), QString());
        backend.playing();
        QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM starts").toInt(), 0);
        QCOMPARE(storedValue(database, "SELECT state FROM runtime_state").toByteArray(), savedState);
        emit backend.positionChanged(4321);
        now = now.addSecs(1);
        engine.evaluateSchedule(now);
        QVERIFY(backend.path.endsWith("5.mp3"));
        backend.playing();
        QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM starts").toInt(), 1);
        backend.finish();
        QTRY_VERIFY(backend.path.endsWith("1.mp3"));
        backend.playing();
        backend.playing();
        QCOMPARE(backend.seekPosition, 4321);
        QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM starts").toInt(), 1);
        QCOMPARE(storedValue(database, "SELECT state FROM runtime_state").toByteArray(), savedState);
        backend.finish();
        QTRY_VERIFY(backend.path.endsWith("3.mp3"));
    }

    void consumeOnlyConfirmedStart()
    {
        QTemporaryDir directory;
        files(directory);
        ScheduleV1Runtime runtime(directory.filePath("runtime.sqlite"), testClock);
        QCOMPARE(runtime.accept(bytes(schedule()), directory.path()), QString());
        const auto now = at("2026-12-16T11:00:00Z");
        auto first = runtime.selectMusic(now);
        QVERIFY(first.path.endsWith("1.mp3"));
        QCOMPARE(runtime.selectMusic(now).path, first.path);
        QCOMPARE(runtime.confirmStarted(first), QString());
        QCOMPARE(runtime.confirmStarted(first), QString());
        auto second = runtime.selectMusic(now);
        QVERIFY(second.path.endsWith("3.mp3"));
        QCOMPARE(runtime.confirmStarted(second), QString());
        QVERIFY(runtime.selectMusic(now).path.endsWith("2.mp3"));
    }

    void failedAndUnavailableSourcesDoNotConsumeSlot()
    {
        QTemporaryDir directory;
        files(directory);
        ScheduleV1Runtime runtime(directory.filePath("runtime.sqlite"), testClock);
        QCOMPARE(runtime.accept(bytes(schedule()), directory.path()), QString());
        const auto now = at("2026-12-16T11:00:00Z");
        auto first = runtime.selectMusic(now);
        auto replacement = runtime.selectMusic(now, {first.path});
        QVERIFY(replacement.path.endsWith("2.mp3"));
        QCOMPARE(runtime.confirmStarted(replacement), QString());
        QVERIFY(QFile::remove(directory.filePath("3.mp3")));
        QVERIFY(QFile::remove(directory.filePath("4.mp3")));
        auto substitution = runtime.selectMusic(now);
        QVERIFY(substitution.path.endsWith("1.mp3"));
        QVERIFY(runtime.diagnostic().contains(QStringLiteral("Дополнительный")));
        QCOMPARE(runtime.confirmStarted(substitution), QString());
        QVERIFY(runtime.selectMusic(now).path.endsWith("2.mp3"));
    }

    void phaseSurvivesRestartRepublicationAndMidnight()
    {
        QTemporaryDir directory;
        files(directory);
        const QString database = directory.filePath("runtime.sqlite");
        {
            ScheduleV1Runtime runtime(database, testClock);
            QCOMPARE(runtime.accept(bytes(schedule()), directory.path()), QString());
            const auto first = runtime.selectMusic(at("2026-12-16T23:59:59Z"));
            QCOMPARE(runtime.confirmStarted(first), QString());
        }
        ScheduleV1Runtime runtime(database, testClock);
        QCOMPARE(runtime.restore(), QString());
        auto document = schedule();
        document.insert("publicationId", id(4));
        document.insert("revision", 2);
        QCOMPARE(runtime.accept(bytes(document), directory.path()), QString());
        auto next = runtime.selectMusic(at("2026-12-17T00:00:01Z"));
        QVERIFY(next.path.endsWith("3.mp3"));
        QCOMPARE(runtime.confirmStarted(next), QString());
        QVERIFY(runtime.selectMusic(at("2026-12-17T00:00:02Z")).path.endsWith("2.mp3"));
    }

    void phaseSurvivesShiftedValidityBoundary()
    {
        QTemporaryDir directory;
        files(directory);
        const auto now = at("2026-12-17T12:30:00Z");
        ScheduleV1Runtime runtime(directory.filePath("runtime.sqlite"), testClock);
        auto document = schedule();
        QCOMPARE(runtime.accept(bytes(document), directory.path(), {}, now), QString());
        QCOMPARE(runtime.confirmStarted(runtime.selectMusic(now)), QString());
        document.insert("revision", 2);
        document.insert("publicationId", id(4));
        document.insert("validity", QJsonObject{{"from", "2026-12-17"}, {"until", "2027-01-16"}});
        QCOMPARE(runtime.accept(bytes(document), directory.path(), {}, now), QString());
        QVERIFY(runtime.selectMusic(now).path.endsWith("3.mp3"));
    }

    void startupRestoresDesiredModeAndUserStopDisablesIt()
    {
        QTemporaryDir directory;
        files(directory);
        const auto now = at("2026-12-16T11:00:00Z");
        const QString database = directory.filePath("runtime.sqlite");
        {
            Backend backend;
            PlayerEngine engine(&backend, nullptr, [&] { return now; }, database);
            QVERIFY(publication(engine, schedule(), directory).value("ok").toBool());
            backend.playing();
            engine.setPlaybackAvailable(false); // orderly process shutdown
        }
        {
            Backend backend;
            PlayerEngine engine(&backend, nullptr, [&] { return now; }, database);
            QCOMPARE(engine.restoreScheduledPlayback(), QString());
            QVERIFY(backend.path.endsWith("3.mp3"));
            QVERIFY(engine.execute({{"command", "stop"}}).value("ok").toBool());
        }
        Backend backend;
        PlayerEngine engine(&backend, nullptr, [&] { return now; }, database);
        QCOMPARE(engine.restoreScheduledPlayback(), QString());
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("manual"));
        QCOMPARE(backend.plays, 0);
        QCOMPARE(engine.status().value("publicationId").toString(), id(3));
    }

    void stopReportsWriteFailureAndRetriesInManualMode()
    {
        QTemporaryDir directory;
        files(directory);
        const auto now = at("2026-12-16T11:00:00Z");
        const QString database = directory.filePath("runtime.sqlite");
        {
            Backend backend;
            PlayerEngine engine(&backend, nullptr, [&] { return now; }, database);
            QVERIFY(publication(engine, schedule(), directory).value("ok").toBool());
            backend.playing();
            QCOMPARE(executeSql(database, "CREATE TRIGGER fail_disable BEFORE INSERT ON preferences "
                "WHEN NEW.name='scheduledPlayback' AND NEW.value=0 "
                "BEGIN SELECT RAISE(FAIL,'injected disable failure'); END"), QString());

            const auto response = engine.execute({{"command", "stop"}});
            QVERIFY(!response.value("ok").toBool());
            QCOMPARE(response.value("error").toObject().value("code").toString(), QStringLiteral("runtime_error"));
            QVERIFY(response.value("error").toObject().value("message").toString().contains("injected disable failure"));
            QCOMPARE(engine.status().value("state").toString(), QStringLiteral("stopped"));
            QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("manual"));
            QVERIFY(!engine.status().value("playbackRequested").toBool());
            const int plays = backend.plays;
            for (int tick = 1; tick <= 10; ++tick)
                engine.evaluateSchedule(now.addSecs(tick));
            QCOMPARE(backend.plays, plays);
            QCOMPARE(storedValue(database, "SELECT value FROM preferences WHERE name='scheduledPlayback'").toInt(), 1);

            QCOMPARE(executeSql(database, "DROP TRIGGER fail_disable"), QString());
            // The normal timer continues retrying even though playback is manual.
            QTRY_COMPARE(storedValue(database, "SELECT value FROM preferences WHERE name='scheduledPlayback'"), QVariant(0));
            QCOMPARE(engine.status().value("scheduleError").toString(), QString());
            QCOMPARE(engine.status().value("state").toString(), QStringLiteral("stopped"));
            QCOMPARE(backend.plays, plays);
        }
        Backend backend;
        PlayerEngine restored(&backend, nullptr, [&] { return now; }, database);
        QCOMPARE(restored.restoreScheduledPlayback(), QString());
        QCOMPARE(restored.status().value("playbackMode").toString(), QStringLiteral("manual"));
        QCOMPARE(backend.plays, 0);
    }

    void explicitScheduleSupersedesPendingStopPersistence()
    {
        QTemporaryDir directory;
        files(directory);
        const auto now = at("2026-12-16T11:00:00Z");
        const QString database = directory.filePath("runtime.sqlite");
        Backend backend;
        PlayerEngine engine(&backend, nullptr, [&] { return now; }, database);
        QVERIFY(publication(engine, schedule(), directory).value("ok").toBool());
        backend.playing();
        QCOMPARE(executeSql(database, "CREATE TRIGGER fail_disable BEFORE INSERT ON preferences "
            "WHEN NEW.value=0 BEGIN SELECT RAISE(FAIL,'injected disable failure'); END"), QString());
        QVERIFY(!engine.execute({{"command", "stop"}}).value("ok").toBool());
        QCOMPARE(executeSql(database, "DROP TRIGGER fail_disable"), QString());
        QVERIFY(engine.execute({{"command", "schedule"}}).value("ok").toBool());
        backend.playing();
        for (int tick = 1; tick <= 10; ++tick)
            engine.evaluateSchedule(now.addSecs(tick));
        QCOMPARE(storedValue(database, "SELECT value FROM preferences WHERE name='scheduledPlayback'").toInt(), 1);
        QCOMPARE(engine.status().value("playbackMode").toString(), QStringLiteral("schedule"));
        QCOMPARE(engine.status().value("state").toString(), QStringLiteral("playing"));
    }

    void advertStartWriteFailureRecoversWithoutReplay_data()
    {
        QTest::addColumn<bool>("interruptMusic");
        QTest::addColumn<QString>("failureStatement");
        const QString starts = QStringLiteral("CREATE TRIGGER fail_ack BEFORE INSERT ON starts "
            "BEGIN SELECT RAISE(FAIL,'injected acknowledgement failure'); END");
        const QString event = QStringLiteral("CREATE TRIGGER fail_ack BEFORE UPDATE OF state ON events "
            "WHEN NEW.state='started' BEGIN SELECT RAISE(FAIL,'injected acknowledgement failure'); END");
        QTest::newRow("initial-start-record") << false << starts;
        QTest::newRow("interrupted-start-record") << true << starts;
        QTest::newRow("initial-event-state") << false << event;
        QTest::newRow("interrupted-event-state") << true << event;
    }

    void advertStartWriteFailureRecoversWithoutReplay()
    {
        QFETCH(bool, interruptMusic);
        QFETCH(QString, failureStatement);
        QTemporaryDir directory;
        files(directory);
        auto now = at(interruptMusic ? "2026-12-16T11:59:59Z" : "2026-12-16T12:00:00Z");
        const QString database = directory.filePath("runtime.sqlite");
        auto document = schedule();
        addEvent(document, "interrupt");
        Backend backend;
        PlayerEngine engine(&backend, nullptr, [&] { return now; }, database);
        QVERIFY(publication(engine, document, directory).value("ok").toBool());
        if (interruptMusic) {
            backend.playing();
            backend.seek(4000);
            now = now.addSecs(1);
            engine.evaluateSchedule(now);
        }
        QVERIFY(backend.path.endsWith("5.mp3"));
        QCOMPARE(executeSql(database, failureStatement), QString());
        QCOMPARE(executeSql(database, "CREATE TRIGGER fail_retire BEFORE UPDATE OF state ON events "
            "WHEN NEW.state='failed' BEGIN SELECT RAISE(FAIL,'injected retirement failure'); END"), QString());
        backend.playing();
        QCOMPARE(engine.status().value("state").toString(), QStringLiteral("stopped"));
        QVERIFY(engine.status().value("scheduleError").toString().contains("injected acknowledgement failure"));
        const int plays = backend.plays;
        for (int tick = 1; tick <= 10; ++tick)
            engine.evaluateSchedule(now.addSecs(tick));
        QCOMPARE(backend.plays, plays);
        QCOMPARE(storedValue(database, "SELECT state FROM events").toString(), QStringLiteral("starting"));
        QCOMPARE(storedValue(database, "SELECT COUNT(*) FROM starts WHERE entry_id=''").toInt(), 0);
        QVERIFY(engine.status().value("scheduleError").toString().contains("injected retirement failure"));
        QCOMPARE(executeSql(database, "DROP TRIGGER fail_ack"), QString());
        QCOMPARE(executeSql(database, "DROP TRIGGER fail_retire"), QString());
        engine.evaluateSchedule(now.addSecs(11));
        QCOMPARE(backend.plays, plays + 1);
        QVERIFY(backend.path.endsWith("1.mp3"));
        backend.playing();
        QCOMPARE(backend.seekPosition, interruptMusic ? qint64(4000) : qint64(0));
        QCOMPARE(engine.status().value("state").toString(), QStringLiteral("playing"));
        QCOMPARE(engine.status().value("scheduleError").toString(), QString());
        QCOMPARE(storedValue(database, "SELECT state FROM events").toString(), QStringLiteral("failed"));
        for (int tick = 12; tick <= 20; ++tick)
            engine.evaluateSchedule(now.addSecs(tick));
        QCOMPARE(backend.plays, plays + 1);
        engine.setPlaybackAvailable(false);
        Backend restartedBackend;
        PlayerEngine restarted(&restartedBackend, nullptr, [&] { return now; }, database);
        QCOMPARE(restarted.restoreScheduledPlayback(), QString());
        QVERIFY(restartedBackend.path.endsWith("3.mp3"));
    }

    void shuffleRemainingCycleSurvivesRestart()
    {
        QTemporaryDir directory;
        files(directory);
        auto document = schedule();
        auto playlists = document.value("playlists").toArray();
        auto base = playlists.at(0).toObject();
        base.insert("order", "shuffle_cycle");
        playlists.replace(0, base);
        document.insert("playlists", playlists);
        document.insert("mixRules", QJsonArray{});
        document.insert("requiredCapabilities", QJsonArray{"calendar.v1"});
        QString first;
        const auto now = at("2026-12-16T11:00:00Z");
        {
            ScheduleV1Runtime runtime(directory.filePath("runtime.sqlite"), testClock);
            QCOMPARE(runtime.accept(bytes(document), directory.path()), QString());
            const auto track = runtime.selectMusic(now);
            first = track.entryId;
            QCOMPARE(runtime.selectMusic(now).entryId, first);
            QCOMPARE(runtime.confirmStarted(track), QString());
        }
        ScheduleV1Runtime restored(directory.filePath("runtime.sqlite"), testClock);
        QCOMPARE(restored.restore(), QString());
        auto next = restored.selectMusic(now);
        QVERIFY(next.entryId != first);
    }

    void rejectionKeepsAcceptedSnapshot()
    {
        QTemporaryDir directory;
        files(directory);
        ScheduleV1Runtime runtime(directory.filePath("runtime.sqlite"), testClock);
        auto document = schedule();
        const auto data = bytes(document);
        const auto pointer = active(document, data);
        QCOMPARE(runtime.accept(data, directory.path(), pointer), QString());
        QCOMPARE(runtime.accept(data, directory.path(), pointer), QString());
        auto invalidPointer = pointer;
        invalidPointer.insert("sha256", QString(64, QLatin1Char('0')));
        QVERIFY(!runtime.accept(data, directory.path(), invalidPointer).isEmpty());
        document.insert("publishedAt", "2026-10-04T10:00:00Z");
        QVERIFY(!runtime.accept(bytes(document), directory.path()).isEmpty());
        document.insert("publicationId", id(4));
        QVERIFY(!runtime.accept(bytes(document), directory.path()).isEmpty());
        QCOMPARE(runtime.document().publicationId(), id(3));
        document.insert("revision", 2);
        QCOMPARE(runtime.accept(bytes(document), directory.path()), QString());
        QVERIFY(!runtime.accept(data, directory.path(), pointer).isEmpty());
        QCOMPARE(runtime.document().publicationId(), id(4));
    }

    void finishTrackAndDuplicatePlaying()
    {
        QTemporaryDir directory;
        files(directory);
        auto now = at("2026-12-16T23:59:59Z");
        Backend backend;
        PlayerEngine engine(&backend, nullptr, [&] { return now; }, directory.filePath("runtime.sqlite"));
        auto document = schedule();
        QVERIFY(publication(engine, document, directory).value("ok").toBool());
        QVERIFY(backend.path.endsWith("1.mp3"));
        backend.playing();
        backend.playing();
        const int calls = backend.loads;
        now = now.addSecs(2);
        engine.evaluateSchedule(now);
        QCOMPARE(backend.loads, calls);
        document.insert("revision", 2);
        document.insert("publicationId", id(4));
        QVERIFY(publication(engine, document, directory, false).value("ok").toBool());
        QCOMPARE(backend.loads, calls);
        backend.finish();
        QTRY_VERIFY(backend.path.endsWith("3.mp3"));
        backend.playing();
        backend.finish();
        QTRY_VERIFY(backend.path.endsWith("2.mp3"));
        QCOMPARE(engine.status().value("publicationId").toString(), id(4));
    }

    void failedStartUsesNextEntryWithoutAdvancingMix()
    {
        QTemporaryDir directory;
        files(directory);
        auto now = at("2026-12-16T11:00:00Z");
        Backend backend;
        PlayerEngine engine(&backend, nullptr, [&] { return now; }, directory.filePath("runtime.sqlite"));
        QVERIFY(publication(engine, schedule(), directory).value("ok").toBool());
        backend.fail();
        QTRY_VERIFY(backend.path.endsWith("2.mp3"));
        backend.playing();
        backend.finish();
        QTRY_VERIFY(backend.path.endsWith("3.mp3"));
    }

    void interruptResumesPositionWithoutDoubleConsumption()
    {
        QTemporaryDir directory;
        files(directory);
        auto now = at("2026-12-16T11:59:59Z");
        auto document = schedule();
        addEvent(document, "interrupt");
        Backend backend;
        PlayerEngine engine(&backend, nullptr, [&] { return now; }, directory.filePath("runtime.sqlite"));
        QVERIFY(publication(engine, document, directory).value("ok").toBool());
        backend.playing();
        emit backend.positionChanged(4321);
        now = now.addSecs(1);
        engine.evaluateSchedule(now);
        QVERIFY(backend.path.endsWith("5.mp3"));
        backend.playing();
        backend.finish();
        QTRY_VERIFY(backend.path.endsWith("1.mp3"));
        backend.playing();
        QCOMPARE(backend.seekPosition, 4321);
        backend.finish();
        QTRY_VERIFY(backend.path.endsWith("3.mp3"));
        engine.evaluateSchedule(now);
        QVERIFY(backend.path.endsWith("3.mp3"));
    }

    void afterTrackExpiresAndClockRollbackDoesNotReplay()
    {
        QTemporaryDir directory;
        files(directory);
        auto now = at("2026-12-16T11:59:59Z");
        auto document = schedule();
        addEvent(document, "after_track", 10);
        Backend backend;
        PlayerEngine engine(&backend, nullptr, [&] { return now; }, directory.filePath("runtime.sqlite"));
        QVERIFY(publication(engine, document, directory).value("ok").toBool());
        backend.playing();
        now = now.addSecs(1);
        engine.evaluateSchedule(now);
        QVERIFY(backend.path.endsWith("1.mp3"));
        now = now.addSecs(11);
        backend.finish();
        QTRY_VERIFY(backend.path.endsWith("3.mp3"));
        backend.playing();
        now = now.addSecs(-11);
        engine.evaluateSchedule(now);
        backend.finish();
        QTRY_VERIFY(backend.path.endsWith("2.mp3"));
    }

    void eventPriorityRecoveryAndRepublicationDedupe()
    {
        QTemporaryDir directory;
        files(directory);
        auto document = schedule();
        addEvent(document, "interrupt", 60, 701, 1);
        addEvent(document, "interrupt", 60, 702, 2);
        const auto now = at("2026-12-16T12:00:00Z");
        {
            ScheduleV1Runtime runtime(directory.filePath("runtime.sqlite"), testClock);
            QCOMPARE(runtime.accept(bytes(document), directory.path()), QString());
            const auto events = runtime.dueEvents(now);
            QCOMPARE(events.size(), 2);
            QCOMPARE(events.first().ruleId, id(702));
            const auto first = runtime.startEvent(events.first(), now);
            QVERIFY(first.isValid()); // crash in starting before any acknowledgement
            const auto second = runtime.startEvent(events.last(), now);
            QCOMPARE(runtime.confirmStarted(second), QString());
            QCOMPARE(runtime.finishEvent(second, "completed"), QString());
        }
        ScheduleV1Runtime restored(directory.filePath("runtime.sqlite"), testClock);
        QCOMPARE(restored.restore(), QString());
        QVERIFY(restored.diagnostic().contains(QStringLiteral("перезапуска")));
        QVERIFY(restored.dueEvents(now).isEmpty());
        document.insert("revision", 2);
        document.insert("publicationId", id(4));
        QCOMPARE(restored.accept(bytes(document), directory.path()), QString());
        QVERIFY(restored.dueEvents(now).isEmpty());
    }

    void zeroLatenessMeansTheScheduledSecond()
    {
        QTemporaryDir directory;
        files(directory);
        auto document = schedule();
        addEvent(document, "interrupt", 0);
        ScheduleV1Runtime runtime(directory.filePath("runtime.sqlite"), testClock);
        QCOMPARE(runtime.accept(bytes(document), directory.path()), QString());
        const auto now = at("2026-12-16T12:00:00Z").addMSecs(500);
        const auto events = runtime.dueEvents(now);
        QCOMPARE(events.size(), 1);
        QVERIFY(runtime.startEvent(events.first(), now).isValid());
        const auto nextDay = now.addDays(1).addMSecs(500);
        QVERIFY(runtime.dueEvents(nextDay).isEmpty());
    }

    void eventPersistenceFailureRemainsDiagnosable()
    {
        QTemporaryDir directory;
        files(directory);
        auto document = schedule();
        addEvent(document, "interrupt");
        const QString path = directory.filePath("runtime.sqlite");
        ScheduleV1Runtime runtime(path, testClock);
        QCOMPARE(runtime.accept(bytes(document), directory.path()), QString());
        const auto now = at("2026-12-16T12:00:00Z");
        const auto events = runtime.dueEvents(now);
        QCOMPARE(events.size(), 1);
        const auto event = runtime.startEvent(events.first(), now);
        QCOMPARE(runtime.confirmStarted(event), QString());
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "event-failure-test");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            {
                QSqlQuery query(db);
                QVERIFY(query.exec("CREATE TRIGGER fail_completed BEFORE UPDATE OF state ON events "
                    "WHEN NEW.state='completed' BEGIN SELECT RAISE(FAIL,'injected write failure'); END"));
            }
            db.close();
        }
        QSqlDatabase::removeDatabase("event-failure-test");
        QVERIFY(runtime.finishEvent(event, "completed").contains("injected write failure"));
        QVERIFY(runtime.selectMusic(now).isValid());
        QVERIFY(runtime.diagnostic().contains("injected write failure"));
    }

    void filePublicationSupportsLargeDocumentsAndRejectsBrokenReplacements()
    {
        QTemporaryDir directory;
        files(directory);
        auto document = schedule();
        auto lists = document.value("playlists").toArray();
        auto list = lists.first().toObject();
        list.insert("name", QString(1100000, u'x'));
        lists.replace(0, list);
        document.insert("playlists", lists);
        QVERIFY(bytes(document).size() > 1024 * 1024);
        const QString path = publishFiles(directory.path(), document);
        QVERIFY(!path.isEmpty());
        Backend backend;
        const auto now = at("2026-12-16T11:00:00Z");
        PlayerEngine engine(&backend, nullptr, [&] { return now; }, directory.filePath("runtime.sqlite"));
        const QJsonObject request{{"command", "loadPublication"}, {"activePath", path},
            {"contentRoot", directory.path()}, {"autoplay", true}};
        QVERIFY(bytes(request).size() < 4096);
        QVERIFY(engine.execute(request).value("ok").toBool());
        backend.playing();
        const auto before = engine.status();
        auto replacement = document;
        replacement.insert("publicationId", id(4));
        replacement.insert("revision", 2);
        QVERIFY(!publishFiles(directory.path(), replacement, true).isEmpty());
        QVERIFY(!engine.execute(request).value("ok").toBool());
        QCOMPARE(engine.status(), before);
        replacement.insert("schemaVersion", 2);
        QVERIFY(!publishFiles(directory.path(), replacement).isEmpty());
        QVERIFY(!engine.execute(request).value("ok").toBool());
        QCOMPARE(engine.status(), before);
        const auto unsupported = engine.execute({{"command", "unrecognizedCommand"}});
        QCOMPARE(unsupported.value("error").toObject().value("code").toString(), QStringLiteral("unknown_command"));
        QCOMPARE(engine.status(), before);
        const auto invalidFields = engine.execute({{"command", "schedule"}, {"payload", document}});
        QCOMPARE(invalidFields.value("error").toObject().value("code").toString(), QStringLiteral("invalid_arguments"));
        QCOMPARE(engine.status(), before);
    }

    void restartReadsLatestFileAndRetainsAcceptedOnFileFailure()
    {
        QTemporaryDir directory;
        files(directory);
        auto now = at("2026-12-16T11:00:00Z");
        const QString database = directory.filePath("runtime.sqlite");
        auto document = schedule();
        {
            Backend backend;
            PlayerEngine engine(&backend, nullptr, [&] { return now; }, database);
            QVERIFY(publication(engine, document, directory).value("ok").toBool());
        }
        document.insert("publicationId", id(4));
        document.insert("revision", 2);
        QVERIFY(!publishFiles(directory.path(), document).isEmpty());
        {
            Backend backend;
            PlayerEngine restarted(&backend, nullptr, [&] { return now; }, database);
            QCOMPARE(restarted.restoreScheduledPlayback(false), QString());
            QCOMPARE(restarted.status().value("revision").toInt(), 2);
            QCOMPARE(backend.plays, 0);
            QCOMPARE(restarted.status().value("playbackMode").toString(), QStringLiteral("manual"));
        }
        QVERIFY(QFile::remove(directory.filePath("active.json")));
        Backend backend;
        PlayerEngine restarted(&backend, nullptr, [&] { return now; }, database);
        QCOMPARE(restarted.restoreScheduledPlayback(), QString());
        QCOMPARE(restarted.status().value("revision").toInt(), 2);
        QVERIFY(!restarted.status().value("scheduleError").toString().isEmpty());
        QCOMPARE(backend.plays, 1);
    }

    void playerMediaCapabilitiesValidateBeforeReplacingAcceptedPublication()
    {
        QTemporaryDir directory;
        files(directory);
        auto document = schedule();
        Backend audioBackend, videoBackend;
        const auto now = at("2026-12-16T11:00:00Z");
        PlayerEngine audio(&audioBackend, nullptr, [&] { return now; }, ":memory:");
        PlayerEngine video(&videoBackend, nullptr, [&] { return now; }, ":memory:", "video");
        QVERIFY(publication(audio, document, directory).value("ok").toBool());
        const auto before = audio.status();
        auto assets = document.value("assets").toArray();
        auto asset = assets.first().toObject();
        asset.insert("mediaType", "video");
        assets.replace(0, asset);
        document.insert("assets", assets);
        document.insert("requiredCapabilities", QJsonArray::fromStringList(ScheduleV1::requiredCapabilities(document)));
        document.insert("publicationId", id(4));
        document.insert("revision", 2);
        QVERIFY(!publication(audio, document, directory).value("ok").toBool());
        QCOMPARE(audio.status(), before);
        QVERIFY(publication(video, document, directory).value("ok").toBool());
        QVERIFY(audio.status().value("supportedCapabilities").toArray().contains("schedule.current.v1"));
        QVERIFY(!audio.status().value("supportedCapabilities").toArray().contains("media.video.v1"));
        QVERIFY(video.status().value("supportedCapabilities").toArray().contains("media.video.v1"));
    }

    void publicationTransportAndPathValidation()
    {
        QTemporaryDir directory;
        files(directory);
        auto now = at("2026-12-16T11:00:00Z");
        auto document = schedule();
        Backend backend;
        PlayerEngine engine(&backend, nullptr, [&] { return now; }, directory.filePath("runtime.sqlite"));
        QVERIFY(publication(engine, document, directory).value("ok").toBool());
        QVERIFY(backend.path.endsWith("1.mp3"));
        auto assets = document.value("assets").toArray();
        auto asset = assets.at(0).toObject();
        asset.insert("path", "../outside.mp3");
        assets.replace(0, asset);
        document.insert("assets", assets);
        document.insert("publicationId", id(4));
        document.insert("revision", 2);
        auto rejected = publication(engine, document, directory, false);
        QVERIFY(!rejected.value("ok").toBool());
        QCOMPARE(engine.status().value("publicationId").toString(), id(3));
    }
};

QTEST_GUILESS_MAIN(ScheduleV1RuntimeTest)
#include "tst_schedulev1runtime.moc"
