#include "schedulepublication.h"
#include "schedulecore/schedulev1.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

namespace {
QJsonObject channels(const QString &root, int offset = 1, const QString &start = "00:00", const QString &end = "00:00")
{
    return {{"channels", QJsonArray{QJsonObject{{"id", "00000000-0000-4000-8000-000000000111"},
        {"name", "Музыка"}, {"start", start}, {"end", end}, {"untilDayOffset", offset},
        {"weekdays", "*"}, {"days", "*"}, {"months", "*"}, {"volume", 70}, {"order", "sequential"},
        {"paths", QJsonArray{QDir(root).filePath("music/channel/трек.mp3")}}}}}, {"adverts", QJsonArray{}}};
}
QByteArray read(const QString &path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
bool writeObject(const QString &path, const QJsonObject &object)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(object).toJson()) >= 0;
}
QString id(int n) { return QStringLiteral("00000000-0000-4000-8000-%1").arg(n, 12, 10, QLatin1Char('0')); }
}

class SchedulePublicationTests : public QObject {
    Q_OBJECT
private slots:
    void compileAndPublish();
    void unchangedReleaseAndPlaylistRevision();
    void explicitOvernightAndInvalidIntervals();
    void advancedDraftIsAuthoritative();
    void legacyAdvancedChannelsReconcileWithoutReplacingCustomRules();
    void advancedChangesPreserveProjectEdits();
    void advancedEditsPreserveEntryPositionsAndDeletedRules();
    void advancedMediaOrderNameAndRemoval_data();
    void advancedMediaOrderNameAndRemoval();
    void advancedOverlapCannotReplaceActivePublication();
    void removedChannelRetainsCustomDependencies();
    void invalidAdvancedChannelInputClearsResult();
    void rejectedPublicationPreservesPointer();
    void contentEscapeRejected();
    void distinctPathsAndRepeatedEntries();
    void renamePreservesEntryIdsAndRevision();
    void generatedHorizonRenewsButAdvancedRangeRemains();
    void videoUsesCurrentPublicationFiles();
};

void SchedulePublicationTests::videoUsesCurrentPublicationFiles()
{
    QTemporaryDir dir;
    auto input = channels(dir.path());
    auto channels = input.value("channels").toArray();
    auto channel = channels[0].toObject();
    channel.insert("paths", QJsonArray{dir.filePath("video/Экран/ролик.mp4")});
    channels[0] = channel;
    input.insert("channels", channels);
    QJsonObject document;
    bool advanced = false;
    QString error;
    const QString project = dir.filePath("video-schedule");
    QVERIFY2(SchedulePublication::draft(project, dir.path(), input, &document, &advanced, &error, "video"), qPrintable(error));
    QCOMPARE(document.value("schemaVersion"), QJsonValue(1));
    QCOMPARE(document.value("assets").toArray().first().toObject().value("mediaType"), QJsonValue("video"));
    QVERIFY(document.value("requiredCapabilities").toArray().contains("media.video.v1"));
    ScheduleV1::Document parsed;
    QVERIFY2(ScheduleV1::decode(document, &parsed).isEmpty(), qPrintable(ScheduleV1::decode(document, &parsed)));
    SchedulePublication::Publication publication;
    QVERIFY2(SchedulePublication::publish(project, dir.path(), document, &publication, &error), qPrintable(error));
    QCOMPARE(publication.activePath, QDir(project).filePath("active.json"));
    QCOMPARE(publication.contentRoot, dir.path());
    const auto pointer = QJsonDocument::fromJson(read(publication.activePath)).object();
    QCOMPARE(pointer.value("sha256"), publication.active.value("sha256"));
    QCOMPARE(read(QDir(project).filePath(pointer.value("snapshotPath").toString())), publication.bytes);
}

void SchedulePublicationTests::compileAndPublish()
{
    QTemporaryDir dir;
    QJsonObject document;
    bool advanced = true;
    QString error;
    QVERIFY2(SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path()), &document, &advanced, &error), qPrintable(error));
    QVERIFY(!advanced);
    ScheduleV1::Document parsed;
    QVERIFY2(ScheduleV1::decode(document, &parsed).isEmpty(), qPrintable(ScheduleV1::decode(document, &parsed)));
    QCOMPARE(document["assets"].toArray().first().toObject()["path"].toString(), QStringLiteral("music/channel/трек.mp3"));
    QCOMPARE(document["baseRules"].toArray().first().toObject()["when"].toObject()["select"].toObject()["weekdays"].toArray(),
             QJsonArray({1, 2, 3, 4, 5, 6, 7}));
    SchedulePublication::Publication release;
    QVERIFY2(SchedulePublication::publish(dir.path(), dir.path(), document, &release, &error), qPrintable(error));
    QCOMPARE(release.active["revision"].toInt(), 1);
    QCOMPARE(read(QDir(dir.path()).filePath(release.active["snapshotPath"].toString())), release.bytes);
    QCOMPARE(release.active["sha256"].toString(), QString::fromLatin1(QCryptographicHash::hash(release.bytes, QCryptographicHash::Sha256).toHex()));
    const auto journal = QJsonDocument::fromJson(read(QDir(dir.path()).filePath("publications/" + release.active["publicationId"].toString() + ".json"))).object();
    QCOMPARE(journal, release.active);
}

void SchedulePublicationTests::unchangedReleaseAndPlaylistRevision()
{
    QTemporaryDir dir;
    QJsonObject document;
    bool advanced;
    QString error;
    auto source = channels(dir.path());
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), source, &document, &advanced, &error));
    SchedulePublication::Publication first, unchanged, next;
    QVERIFY2(SchedulePublication::publish(dir.path(), dir.path(), document, &first, &error), qPrintable(error));
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), source, &document, &advanced, &error));
    QVERIFY(SchedulePublication::publish(dir.path(), dir.path(), document, &unchanged, &error));
    QCOMPARE(unchanged.bytes, first.bytes);
    auto channel = source["channels"].toArray().first().toObject();
    channel["paths"] = QJsonArray{QDir(dir.path()).filePath("music/channel/трек.mp3"), QDir(dir.path()).filePath("music/channel/второй.mp3")};
    source["channels"] = QJsonArray{channel};
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), source, &document, &advanced, &error));
    QCOMPARE(document["playlists"].toArray().first().toObject()["revision"].toInt(), 2);
    QVERIFY(SchedulePublication::publish(dir.path(), dir.path(), document, &next, &error));
    QCOMPARE(next.active["revision"].toInt(), 2);
    QVERIFY(next.active["publicationId"] != first.active["publicationId"]);
    QVERIFY(QFileInfo::exists(QDir(dir.path()).filePath(first.active["snapshotPath"].toString())));
}

void SchedulePublicationTests::explicitOvernightAndInvalidIntervals()
{
    QTemporaryDir dir;
    QJsonObject document;
    bool advanced;
    QString error;
    QVERIFY(!SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path(), 0), &document, &advanced, &error));
    QVERIFY(error.contains(QStringLiteral("Полные сутки")));
    QVERIFY(!SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path(), 0, "22:00", "06:00"), &document, &advanced, &error));
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path(), 1, "22:00", "06:00"), &document, &advanced, &error));
    const auto window = document["dayTemplates"].toArray().first().toObject()["slots"].toArray().first().toObject()["window"].toObject();
    QCOMPARE(window["from"].toString(), QString("22:00:00"));
    QCOMPARE(window["untilDayOffset"].toInt(), 1);
}

void SchedulePublicationTests::advancedDraftIsAuthoritative()
{
    QTemporaryDir dir;
    QJsonObject document, readback;
    bool advanced;
    QString error;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path()), &document, &advanced, &error));
    document["baseRules"] = QJsonArray{};
    QVERIFY2(SchedulePublication::saveDraft(dir.path(), document, &error), qPrintable(error));
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path()), &readback, &advanced, &error));
    QVERIFY(advanced);
    QVERIFY(readback["baseRules"].toArray().isEmpty());
    QCOMPARE(readback["scheduleId"], document["scheduleId"]);
}

void SchedulePublicationTests::legacyAdvancedChannelsReconcileWithoutReplacingCustomRules()
{
    QTemporaryDir dir;
    auto input = channels(dir.path(), 0, "08:00", "22:00");
    auto first = input["channels"].toArray().first().toObject(); first["name"] = "Первый";
    input["channels"] = QJsonArray{first};
    QJsonObject document; bool advanced; QString error;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error));
    const QJsonObject calendar{{"id", id(201)}, {"revision", 1}, {"name", "Календарь проекта"},
        {"coverage", document.value("validity")}, {"dates", QJsonArray{}}};
    document["calendars"] = QJsonArray{calendar};
    auto day = document["dayTemplates"].toArray().first().toObject();
    const QJsonObject customSlot{{"id", id(207)}, {"window", QJsonObject{{"from", "22:00:00"}, {"until", "23:00:00"}, {"untilDayOffset", 0}}},
        {"source", QJsonObject{{"type", "silence"}}}, {"volumePercent", 0}};
    day["slots"] = QJsonArray{customSlot, day["slots"].toArray().first()};
    document["dayTemplates"] = QJsonArray{day};
    QVERIFY2(SchedulePublication::saveDraft(dir.path(), document, &error), qPrintable(error));
    auto envelope = QJsonDocument::fromJson(read(SchedulePublication::projectPath(dir.path()))).object();
    envelope.remove("channelDocument");
    QVERIFY(writeObject(SchedulePublication::projectPath(dir.path()), envelope));
    first["end"] = "14:00";
    auto second = first; second["id"] = id(112); second["name"] = "Второй"; second["start"] = "14:00"; second["end"] = "22:00";
    second["paths"] = QJsonArray{dir.filePath("music/Второй/трек.mp3")};
    input["channels"] = QJsonArray{first, second};
    QVERIFY2(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error), qPrintable(error));
    QVERIFY(advanced);
    QCOMPARE(document["baseRules"].toArray().size(), 2);
    QCOMPARE(document["calendars"].toArray(), QJsonArray{calendar});
    QVERIFY(document["dayTemplates"].toArray().first().toObject()["slots"].toArray().contains(customSlot));
    ScheduleV1::Document parsed;
    const auto reason = ScheduleV1::decode(document, &parsed); QVERIFY2(reason.isEmpty(), qPrintable(reason));
    const QTimeZone zone(document.value("timeZone").toString().toUtf8());
    const QDate previewDay = QDate::fromString(document["validity"].toObject()["from"].toString(), Qt::ISODate);
    QCOMPARE(ScheduleV1::evaluate(parsed, QDateTime(previewDay, QTime(13, 59), zone)).playlistId, id(111));
    QCOMPARE(ScheduleV1::evaluate(parsed, QDateTime(previewDay, QTime(14, 0), zone)).playlistId, id(112));
    const auto migrated = QJsonDocument::fromJson(read(SchedulePublication::projectPath(dir.path()))).object();
    QVERIFY(migrated.value("channelDocument").isObject());
    QJsonObject again;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &again, &advanced, &error));
    QCOMPARE(again, document);
}

void SchedulePublicationTests::advancedChangesPreserveProjectEdits()
{
    QTemporaryDir dir;
    auto input = channels(dir.path(), 0, "08:00", "22:00");
    QJsonObject document; bool advanced; QString error;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error));
    const QJsonObject condition{{"select", QJsonObject{{"type", "all"}}}, {"excludeDates", QJsonArray{}}};
    const QJsonObject window{{"from", "00:00:00"}, {"until", "00:00:00"}, {"untilDayOffset", 1}};
    const QJsonObject mixing{{"id", id(202)}, {"name", "Подмешивание"}, {"enabled", true}, {"priority", 5},
        {"when", condition}, {"windows", QJsonArray{window}}, {"pattern", QJsonArray{
            QJsonObject{{"type", "active_base"}}, QJsonObject{{"type", "playlist"}, {"playlistId", id(111)}}}},
        {"emptyAdditionalSource", "use_base"}};
    const QJsonObject event{{"id", id(203)}, {"name", "Вставка"}, {"enabled", true}, {"priority", 7},
        {"when", condition}, {"times", QJsonArray{"12:34:56"}},
        {"action", QJsonObject{{"assetId", document["assets"].toArray().first().toObject()["id"]}, {"volumePercent", 75}}},
        {"delivery", QJsonObject{{"start", "after_track"}, {"maxLateSeconds", 120}, {"expired", "skip"}, {"after", "resume_music"}}}};
    document["mixRules"] = QJsonArray{mixing}; document["eventRules"] = QJsonArray{event};
    auto rules = document["baseRules"].toArray(); auto rule = rules[0].toObject(); rule["priority"] = 15; rules[0] = rule;
    document["baseRules"] = rules;
    auto templates = document["dayTemplates"].toArray(); auto day = templates[0].toObject();
    auto daySlots = day["slots"].toArray(); auto slot = daySlots[0].toObject();
    auto slotWindow = slot["window"].toObject(); slotWindow["from"] = "08:10:00"; slot["window"] = slotWindow;
    daySlots[0] = slot; day["slots"] = daySlots; templates[0] = day; document["dayTemplates"] = templates;
    document["requiredCapabilities"] = QJsonArray::fromStringList(ScheduleV1::requiredCapabilities(document));
    QVERIFY2(SchedulePublication::saveDraft(dir.path(), document, &error), qPrintable(error));
    QJsonObject unchanged;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &unchanged, &advanced, &error));
    QCOMPARE(unchanged, document);
    auto channel = input["channels"].toArray().first().toObject(); channel["end"] = "20:00"; channel["volume"] = 17; channel["name"] = "Новое имя";
    input["channels"] = QJsonArray{channel};
    QVERIFY2(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error), qPrintable(error));
    QCOMPARE(document["mixRules"].toArray(), QJsonArray{mixing}); QCOMPARE(document["eventRules"].toArray(), QJsonArray{event});
    QCOMPARE(document["baseRules"].toArray().first().toObject()["priority"], QJsonValue(15));
    slot = document["dayTemplates"].toArray().first().toObject()["slots"].toArray().first().toObject();
    QCOMPARE(slot["window"].toObject()["from"], QJsonValue("08:10:00"));
    QCOMPARE(slot["window"].toObject()["until"], QJsonValue("20:00:00"));
    QCOMPARE(slot["volumePercent"], QJsonValue(17));
    QCOMPARE(document["playlists"].toArray().first().toObject()["name"], QJsonValue("Новое имя"));
    ScheduleV1::Document parsed; const auto reason = ScheduleV1::decode(document, &parsed); QVERIFY2(reason.isEmpty(), qPrintable(reason));
}

void SchedulePublicationTests::advancedMediaOrderNameAndRemoval_data()
{
    QTest::addColumn<QString>("mediaType");
    QTest::newRow("audio") << QStringLiteral("audio");
    QTest::newRow("video") << QStringLiteral("video");
}

void SchedulePublicationTests::advancedEditsPreserveEntryPositionsAndDeletedRules()
{
    QTemporaryDir dir;
    auto input = channels(dir.path());
    auto channel = input["channels"].toArray().first().toObject();
    const QString firstPath = dir.filePath("music/channel/first.mp3"), secondPath = dir.filePath("music/channel/second.mp3");
    const QString thirdPath = dir.filePath("music/channel/third.mp3");
    channel["paths"] = QJsonArray{firstPath, secondPath}; input["channels"] = QJsonArray{channel};
    QJsonObject document; bool advanced; QString error;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error));
    auto playlist = document["playlists"].toArray().first().toObject(); const auto original = playlist["entries"].toArray();
    const QJsonObject custom{{"id", id(301)}, {"assetId", original.first().toObject().value("assetId")}};
    playlist["entries"] = QJsonArray{original[0], custom, original[1]}; document["playlists"] = QJsonArray{playlist};
    document["baseRules"] = QJsonArray{};
    QVERIFY(SchedulePublication::saveDraft(dir.path(), document, &error));
    channel["name"] = "Новое имя"; channel["paths"] = QJsonArray{firstPath, secondPath, thirdPath};
    input["channels"] = QJsonArray{channel};
    QVERIFY2(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error), qPrintable(error));
    QVERIFY(document["baseRules"].toArray().isEmpty());
    auto entries = document["playlists"].toArray().first().toObject()["entries"].toArray();
    QCOMPARE(entries.size(), 4); QCOMPARE(entries[0], original[0]); QCOMPARE(entries[1], QJsonValue(custom)); QCOMPARE(entries[2], original[1]);
    channel["paths"] = QJsonArray{secondPath, firstPath, thirdPath}; input["channels"] = QJsonArray{channel};
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error));
    entries = document["playlists"].toArray().first().toObject()["entries"].toArray();
    QCOMPARE(entries[0], original[1]); QCOMPARE(entries[1], QJsonValue(custom)); QCOMPARE(entries[2], original[0]);
    QVERIFY(document["baseRules"].toArray().isEmpty());
}
void SchedulePublicationTests::advancedMediaOrderNameAndRemoval()
{
    QFETCH(QString, mediaType);
    QTemporaryDir dir;
    auto input = channels(dir.path());
    auto channel = input["channels"].toArray().first().toObject();
    const QString folder = mediaType == "video" ? "video/channel/" : "music/channel/";
    channel["paths"] = QJsonArray{dir.filePath(folder + "first.mp3")}; input["channels"] = QJsonArray{channel};
    QJsonObject document; bool advanced; QString error;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error, mediaType));
    auto playlist = document["playlists"].toArray().first().toObject(); playlist["revision"] = 40;
    const auto originalEntry = playlist["entries"].toArray().first().toObject();
    const QJsonObject customEntry{{"id", id(301)}, {"assetId", originalEntry.value("assetId")}};
    playlist["entries"] = QJsonArray{originalEntry, customEntry}; document["playlists"] = QJsonArray{playlist};
    QVERIFY(SchedulePublication::saveDraft(dir.path(), document, &error));
    channel["paths"] = QJsonArray{dir.filePath(folder + "first.mp3"), dir.filePath(folder + "second.mp3")};
    channel["name"] = "Переименованный"; channel["order"] = "shuffle_cycle"; input["channels"] = QJsonArray{channel};
    QVERIFY2(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error, mediaType), qPrintable(error));
    playlist = document["playlists"].toArray().first().toObject();
    QCOMPARE(playlist["revision"], QJsonValue(41)); QCOMPARE(playlist["entries"].toArray().size(), 3);
    QVERIFY(playlist["entries"].toArray().contains(customEntry));
    QCOMPARE(playlist["order"], QJsonValue("shuffle_cycle")); QCOMPARE(playlist["name"], QJsonValue("Переименованный"));
    QCOMPARE(document["assets"].toArray().size(), 2);
    for (const auto &asset : document["assets"].toArray()) QCOMPARE(asset.toObject()["mediaType"], QJsonValue(mediaType));
    channel["paths"] = QJsonArray{dir.filePath(folder + "second.mp3")}; input["channels"] = QJsonArray{channel};
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error, mediaType));
    playlist = document["playlists"].toArray().first().toObject();
    QCOMPARE(playlist["entries"].toArray().size(), 2);
    QVERIFY(playlist["entries"].toArray().contains(customEntry));
    QVERIFY(!playlist["entries"].toArray().contains(originalEntry));
    QCOMPARE(document["assets"].toArray().size(), 2); // Custom entry still references the old asset.
    ScheduleV1::Document parsed; const auto reason = ScheduleV1::decode(document, &parsed); QVERIFY2(reason.isEmpty(), qPrintable(reason));
    input["channels"] = QJsonArray{};
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error, mediaType));
    QVERIFY(document["playlists"].toArray().isEmpty()); QVERIFY(document["baseRules"].toArray().isEmpty());
    QVERIFY(document["dayTemplates"].toArray().isEmpty());
}

void SchedulePublicationTests::advancedOverlapCannotReplaceActivePublication()
{
    QTemporaryDir dir;
    auto input = channels(dir.path(), 0, "08:00", "22:00");
    QJsonObject document; bool advanced; QString error;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error));
    QVERIFY(SchedulePublication::saveDraft(dir.path(), document, &error));
    SchedulePublication::Publication publication;
    QVERIFY(SchedulePublication::publish(dir.path(), dir.path(), document, &publication, &error));
    const auto pointer = read(publication.activePath);
    auto second = input["channels"].toArray().first().toObject(); second["id"] = id(112); second["name"] = "Второй";
    input["channels"] = QJsonArray{input["channels"].toArray().first(), second};
    QVERIFY2(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error), qPrintable(error));
    QCOMPARE(document["baseRules"].toArray().size(), 2);
    ScheduleV1::Document parsed;
    QVERIFY(ScheduleV1::decode(document, &parsed).contains(QStringLiteral("одинаковом приоритете")));
    QVERIFY(!SchedulePublication::publish(dir.path(), dir.path(), document, &publication, &error));
    QCOMPARE(read(publication.activePath), pointer);
    second["start"] = "22:00"; second["end"] = "23:00";
    input["channels"] = QJsonArray{input["channels"].toArray().first(), second};
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error));
    QVERIFY2(SchedulePublication::publish(dir.path(), dir.path(), document, &publication, &error), qPrintable(error));
}

void SchedulePublicationTests::removedChannelRetainsCustomDependencies()
{
    QTemporaryDir dir;
    auto input = channels(dir.path()); QJsonObject document; bool advanced; QString error;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error));
    const auto playlist = document["playlists"].toArray().first();
    const auto asset = document["assets"].toArray().first();
    document["fallback"] = QJsonObject{{"source", QJsonObject{{"type", "playlist"}, {"playlistId", id(111)}}}, {"volumePercent", 40}};
    QVERIFY(SchedulePublication::saveDraft(dir.path(), document, &error));
    input["channels"] = QJsonArray{};
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &document, &advanced, &error));
    QVERIFY(document["baseRules"].toArray().isEmpty()); QVERIFY(document["dayTemplates"].toArray().isEmpty());
    QCOMPARE(document["playlists"].toArray(), QJsonArray{playlist}); QCOMPARE(document["assets"].toArray(), QJsonArray{asset});
    ScheduleV1::Document parsed; const auto reason = ScheduleV1::decode(document, &parsed); QVERIFY2(reason.isEmpty(), qPrintable(reason));
    QJsonObject again;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), input, &again, &advanced, &error)); QCOMPARE(again, document);
}

void SchedulePublicationTests::invalidAdvancedChannelInputClearsResult()
{
    QTemporaryDir dir; QJsonObject document; bool advanced; QString error;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path()), &document, &advanced, &error));
    QVERIFY(SchedulePublication::saveDraft(dir.path(), document, &error));
    const auto project = read(SchedulePublication::projectPath(dir.path()));
    QVERIFY(!SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path(), 0), &document, &advanced, &error));
    QVERIFY(document.isEmpty()); QVERIFY(advanced); QVERIFY(!error.isEmpty());
    QCOMPARE(read(SchedulePublication::projectPath(dir.path())), project);
}

void SchedulePublicationTests::rejectedPublicationPreservesPointer()
{
    QTemporaryDir dir;
    QJsonObject document;
    bool advanced;
    QString error;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path()), &document, &advanced, &error));
    SchedulePublication::Publication publication;
    QVERIFY(SchedulePublication::publish(dir.path(), dir.path(), document, &publication, &error));
    const auto before = read(publication.activePath);
    document["musicTransition"] = "interrupt";
    QVERIFY(!SchedulePublication::publish(dir.path(), dir.path(), document, &publication, &error));
    QCOMPARE(read(publication.activePath), before);
}

void SchedulePublicationTests::contentEscapeRejected()
{
    QTemporaryDir dir;
    QJsonObject source = channels(dir.path()), document;
    auto channel = source["channels"].toArray().first().toObject();
    channel["paths"] = QJsonArray{QDir(dir.path()).absoluteFilePath("../outside.mp3")};
    source["channels"] = QJsonArray{channel};
    bool advanced;
    QString error;
    QVERIFY(!SchedulePublication::draft(dir.path(), dir.path(), source, &document, &advanced, &error));
}
void SchedulePublicationTests::distinctPathsAndRepeatedEntries()
{
    QTemporaryDir dir;
    auto source = channels(dir.path());
    auto channel = source["channels"].toArray().first().toObject();
    const QString first = QDir(dir.path()).filePath("music/channel/a/same.mp3");
    channel["paths"] = QJsonArray{first, QDir(dir.path()).filePath("music/channel/b/same.mp3"), first};
    source["channels"] = QJsonArray{channel};
    QJsonObject document;
    QString error;
    bool advanced;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), source, &document, &advanced, &error));
    QCOMPARE(document["assets"].toArray().size(), 2);
    const auto entries = document["playlists"].toArray().first().toObject()["entries"].toArray();
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries[0].toObject()["assetId"], entries[2].toObject()["assetId"]);
    QVERIFY(entries[0].toObject()["id"] != entries[2].toObject()["id"]);
    ScheduleV1::Document verified;
    QVERIFY2(ScheduleV1::decode(document, &verified).isEmpty(), qPrintable(ScheduleV1::decode(document, &verified)));
}
void SchedulePublicationTests::renamePreservesEntryIdsAndRevision()
{
    QTemporaryDir dir;
    auto source = channels(dir.path());
    auto channel = source["channels"].toArray().first().toObject();
    channel["paths"] = QJsonArray{QDir(dir.path()).filePath("music/Музыка/папка/трек.mp3")};
    source["channels"] = QJsonArray{channel};
    QJsonObject first, renamed;
    QString error;
    bool advanced;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), source, &first, &advanced, &error));
    channel["directory"] = channel.value("name");
    channel["name"] = "Переименованный";
    QCOMPARE(channel.value("directory").toString(), QStringLiteral("Музыка"));
    source["channels"] = QJsonArray{channel};
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), source, &renamed, &advanced, &error));
    const auto before = first["playlists"].toArray().first().toObject();
    const auto after = renamed["playlists"].toArray().first().toObject();
    QCOMPARE(before["entries"], after["entries"]);
    QCOMPARE(before["revision"], after["revision"]);
    QCOMPARE(first["assets"], renamed["assets"]);
    QCOMPARE(after["name"].toString(), QStringLiteral("Переименованный"));
}
void SchedulePublicationTests::generatedHorizonRenewsButAdvancedRangeRemains()
{
    QTemporaryDir dir;
    QJsonObject document;
    QString error;
    bool advanced;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path()), &document, &advanced, &error));
    const QJsonObject expired{{"from", "2020-01-01"}, {"until", "2021-01-01"}};
    document["validity"] = expired;
    auto project = QJsonDocument::fromJson(read(SchedulePublication::projectPath(dir.path()))).object();
    project["document"] = document;
    QFile file(SchedulePublication::projectPath(dir.path()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(project).toJson()); file.close();
    QJsonObject renewed;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path()), &renewed, &advanced, &error));
    QCOMPARE(renewed["validity"].toObject()["from"].toString(), QDate::currentDate().toString(Qt::ISODate));
    QVERIFY(SchedulePublication::saveDraft(dir.path(), document, &error));
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), channels(dir.path()), &renewed, &advanced, &error));
    QVERIFY(advanced);
    QCOMPARE(renewed["validity"].toObject(), expired);
}
QTEST_GUILESS_MAIN(SchedulePublicationTests)
#include "tst_schedulepublication.moc"
