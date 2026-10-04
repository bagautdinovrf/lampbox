#include "schedulepublication.h"
#include "schedulecore/schedulev1.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

namespace {
QJsonObject legacy(const QString &root, int offset = 1, const QString &start = "00:00", const QString &end = "00:00")
{
    return {{"channels", QJsonArray{QJsonObject{{"id", "00000000-0000-4000-8000-000000000111"},
        {"name", "Музыка"}, {"start", start}, {"end", end}, {"untilDayOffset", offset},
        {"weekdays", "*"}, {"days", "*"}, {"months", "*"}, {"volume", 70}, {"order", "sequential"},
        {"paths", QJsonArray{QDir(root).filePath("music/channel/трек.mp3")}}}}}, {"adverts", QJsonArray{}}};
}
QByteArray read(const QString &path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
}

class SchedulePublicationTests : public QObject {
    Q_OBJECT
private slots:
    void migrateAndPublish();
    void unchangedReleaseAndPlaylistRevision();
    void explicitOvernightAndLegacyAmbiguity();
    void advancedDraftIsAuthoritative();
    void rejectedPublicationPreservesPointer();
    void contentEscapeRejected();
    void distinctPathsAndRepeatedEntries();
    void renamePreservesEntryIdsAndRevision();
    void generatedHorizonRenewsButAdvancedRangeRemains();
};

void SchedulePublicationTests::migrateAndPublish()
{
    QTemporaryDir dir;
    QJsonObject document;
    bool advanced = true;
    QString error;
    QVERIFY2(SchedulePublication::draft(dir.path(), dir.path(), legacy(dir.path()), &document, &advanced, &error), qPrintable(error));
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
    QCOMPARE(QByteArray::fromBase64(journal["snapshotBase64"].toString().toLatin1()), release.bytes);
}

void SchedulePublicationTests::unchangedReleaseAndPlaylistRevision()
{
    QTemporaryDir dir;
    QJsonObject document;
    bool advanced;
    QString error;
    auto source = legacy(dir.path());
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

void SchedulePublicationTests::explicitOvernightAndLegacyAmbiguity()
{
    QTemporaryDir dir;
    QJsonObject document;
    bool advanced;
    QString error;
    QVERIFY(!SchedulePublication::draft(dir.path(), dir.path(), legacy(dir.path(), 0), &document, &advanced, &error));
    QVERIFY(error.contains(QStringLiteral("Полные сутки")));
    QVERIFY(!SchedulePublication::draft(dir.path(), dir.path(), legacy(dir.path(), 0, "22:00", "06:00"), &document, &advanced, &error));
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), legacy(dir.path(), 1, "22:00", "06:00"), &document, &advanced, &error));
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
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), legacy(dir.path()), &document, &advanced, &error));
    document["baseRules"] = QJsonArray{};
    QVERIFY2(SchedulePublication::saveDraft(dir.path(), document, &error), qPrintable(error));
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), legacy(dir.path()), &readback, &advanced, &error));
    QVERIFY(advanced);
    QVERIFY(readback["baseRules"].toArray().isEmpty());
    QCOMPARE(readback["scheduleId"], document["scheduleId"]);
}

void SchedulePublicationTests::rejectedPublicationPreservesPointer()
{
    QTemporaryDir dir;
    QJsonObject document;
    bool advanced;
    QString error;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), legacy(dir.path()), &document, &advanced, &error));
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
    QJsonObject source = legacy(dir.path()), document;
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
    auto source = legacy(dir.path());
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
    auto source = legacy(dir.path());
    auto channel = source["channels"].toArray().first().toObject();
    channel["paths"] = QJsonArray{QDir(dir.path()).filePath("music/Музыка/папка/трек.mp3")};
    source["channels"] = QJsonArray{channel};
    QJsonObject first, renamed;
    QString error;
    bool advanced;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), source, &first, &advanced, &error));
    channel["name"] = "Переименованный";
    channel["paths"] = QJsonArray{QDir(dir.path()).filePath("music/Переименованный/папка/трек.mp3")};
    source["channels"] = QJsonArray{channel};
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), source, &renamed, &advanced, &error));
    const auto before = first["playlists"].toArray().first().toObject();
    const auto after = renamed["playlists"].toArray().first().toObject();
    QCOMPARE(before["entries"], after["entries"]);
    QCOMPARE(before["revision"], after["revision"]);
    QCOMPARE(first["assets"].toArray().first().toObject()["id"], renamed["assets"].toArray().first().toObject()["id"]);
}
void SchedulePublicationTests::generatedHorizonRenewsButAdvancedRangeRemains()
{
    QTemporaryDir dir;
    QJsonObject document;
    QString error;
    bool advanced;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), legacy(dir.path()), &document, &advanced, &error));
    const QJsonObject expired{{"from", "2020-01-01"}, {"until", "2021-01-01"}};
    document["validity"] = expired;
    auto project = QJsonDocument::fromJson(read(SchedulePublication::projectPath(dir.path()))).object();
    project["document"] = document;
    QFile file(SchedulePublication::projectPath(dir.path()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(project).toJson()); file.close();
    QJsonObject renewed;
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), legacy(dir.path()), &renewed, &advanced, &error));
    QCOMPARE(renewed["validity"].toObject()["from"].toString(), QDate::currentDate().toString(Qt::ISODate));
    QVERIFY(SchedulePublication::saveDraft(dir.path(), document, &error));
    QVERIFY(SchedulePublication::draft(dir.path(), dir.path(), legacy(dir.path()), &renewed, &advanced, &error));
    QVERIFY(advanced);
    QCOMPARE(renewed["validity"].toObject(), expired);
}
QTEST_GUILESS_MAIN(SchedulePublicationTests)
#include "tst_schedulepublication.moc"
