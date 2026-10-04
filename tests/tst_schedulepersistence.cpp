#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include "advertmanager.h"
#include "advertmodel.h"
#include "channelmanager.h"
#include "channelmodel.h"
#include "projectrepository.h"
#include "projectfixture.h"
#include "schedulepublication.h"
#include "schedulev1runtime.h"
#include "stationmanager.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
void put(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) qFatal("Fixture write failed");
}
QByteArray get(const QString &path)
{
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
QVariantList channelFields(const QString &name = QStringLiteral("One"), int volume = 70)
{
    return {name, QTime(8, 0), QTime(18, 0), QStringLiteral("*"), QStringLiteral("*"), QStringLiteral("*"), volume};
}
QVariantList advertFields(int volume = 70)
{
    return {QStringLiteral("ad.mp3"), QStringLiteral("10"), QStringLiteral("3"), QStringLiteral("*"),
            QDate(2026, 10, 4), QDate(2026, 10, 4), volume};
}
#ifdef Q_OS_WIN
class DenyReplacement {
public:
    explicit DenyReplacement(const QString &path)
        : handle(CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
                             FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {}
    ~DenyReplacement() { if (valid()) CloseHandle(handle); }
    bool valid() const { return handle != INVALID_HANDLE_VALUE; }
private:
    HANDLE handle;
};
#endif
}

class SchedulePersistenceTests : public QObject
{
    Q_OBJECT
    QTemporaryDir station;
    QString projectFile() const { return station.filePath("project.json"); }
    ProjectRepository::Paths paths() const
    {
        return {station.path(), station.filePath("media/music"), station.filePath("media/video")};
    }
    ProjectRepository::Project readProject()
    {
        ProjectRepository::Project project;
        QString error;
        if (!ProjectRepository::load(paths(), &project, &error)) qFatal("Project fixture: %s", qPrintable(error));
        return project;
    }
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(station.isValid());
        QCoreApplication::instance()->setProperty("restylePreviewStation", station.path());
        QCoreApplication::instance()->setProperty("restylePreviewSettings", station.filePath("settings.ini"));
        put(station.filePath("mediabox.conf"), "[mediastation]\nmediabox_id=-1\nmedia=media\n");
        QVERIFY(StationManager::Instance().update());
    }
    void init()
    {
        QFile::remove(projectFile()); QFile::remove(projectFile() + ".pending");
        ProjectRepository::Project project;
        project.music.append(ProjectFixture::channel("One", QTime(8, 0), QTime(18, 0), 70));
        project.video.append(ProjectFixture::channel("Screen", QTime(8, 0), QTime(18, 0), 100));
        project.advert.append(ProjectFixture::advert("ad.mp3", "10", "3",
            QDate(2026, 10, 4), QDate(2026, 10, 4), 70, {2, 22, 42}));
        put(projectFile(), ProjectRepository::encode(project));
        QDir().mkpath(station.filePath("media/music/One"));
        QDir().mkpath(station.filePath("media/video/Screen"));
    }

    void missingProjectIgnoresUnrelatedFiles()
    {
        QFile::remove(projectFile());
        const QString oldMusic = station.filePath("timetable/timetable");
        const QString oldAdvert = station.filePath("timetable/advertView");
        put(oldMusic, "One 08:00 18:00 * * * 70\n");
        put(oldAdvert, "broken obsolete data\n");
        const auto project = readProject();
        QVERIFY(project.music.isEmpty()); QVERIFY(project.video.isEmpty()); QVERIFY(project.advert.isEmpty());
        QCOMPARE(QJsonDocument::fromJson(get(projectFile())).object().value("schemaVersion"), QJsonValue(3));
        QCOMPARE(get(oldMusic), QByteArray("One 08:00 18:00 * * * 70\n"));
        QCOMPARE(get(oldAdvert), QByteArray("broken obsolete data\n"));
        ChannelManager manager(MediaBoxManager::MUSIC);
        QVERIFY(manager.collectChannels());
        QCOMPARE(manager.channelCount(), 0);
        QVERIFY(manager.createChannel(channelFields("Two")));
        QCOMPARE(manager.channelCount(), 1);
        QCOMPARE(readProject().music.first().name, QStringLiteral("Two"));
    }

    void unsupportedProjectVersionsFailWithoutMutation()
    {
        const auto original = QJsonDocument::fromJson(get(projectFile())).object();
        for (int version : {1, 2, 4}) {
            auto object = original;
            object["schemaVersion"] = version;
            const QByteArray bytes = QJsonDocument(object).toJson();
            put(projectFile(), bytes);
            ProjectRepository::Project output;
            QString error;
            QVERIFY(!ProjectRepository::decode(bytes, &output, &error));
            QVERIFY(!error.isEmpty());
            QVERIFY(!ProjectRepository::load(paths(), &output, &error));
            ChannelManager manager(MediaBoxManager::MUSIC);
            QVERIFY(!manager.collectChannels());
            QVERIFY(!manager.createChannel(channelFields("New")));
            QCOMPARE(get(projectFile()), bytes);
            QVERIFY(QDir(station.path()).entryList({"project.json.v*.bak*"}, QDir::Files).isEmpty());
        }
    }

    void channelOrderSurvivesEditsAndReload()
    {
        auto original = readProject();
        QCOMPARE(original.music[0].order, QStringLiteral("shuffle_cycle"));
        QCOMPARE(original.video[0].order, QStringLiteral("shuffle_cycle"));
        ChannelManager music(MediaBoxManager::MUSIC), video(MediaBoxManager::VIDEO);
        QVERIFY(music.collectChannels()); QVERIFY(video.collectChannels());
        QCOMPARE(music.channel(0).playbackOrder(), QStringLiteral("shuffle_cycle"));
        ChannelModel model(&music);
        QVERIFY(model.setData(model.index(0, 0), QStringLiteral("sequential"), ChannelModel::PlaybackOrderRole));
        QVERIFY(model.setData(model.index(0, 6), 45, Qt::EditRole));
        QVERIFY(music.setRule(0, channelFields("One", 46)));
        auto fields = channelFields("Screen", 55); fields.append(QStringLiteral("sequential"));
        QVERIFY(video.setRule(0, fields));
        const auto saved = readProject();
        QCOMPARE(saved.music[0].order, QStringLiteral("sequential"));
        QCOMPARE(saved.video[0].order, QStringLiteral("sequential"));
        QCOMPARE(saved.music[0].volume, 46);
        QCOMPARE(saved.advert[0].compiledMinutes, original.advert[0].compiledMinutes);
        QVERIFY(music.collectChannels()); QVERIFY(video.collectChannels());
        QCOMPARE(music.channel(0).playbackOrder(), QStringLiteral("sequential"));
        QCOMPARE(video.channel(0).playbackOrder(), QStringLiteral("sequential"));
        fields[7] = QStringLiteral("shuffle_cycle");
        QVERIFY(video.setRule(0, fields));
        QCOMPARE(readProject().video[0].order, QStringLiteral("shuffle_cycle"));
        const QByteArray before = get(projectFile());
        fields[7] = QStringLiteral("invalid");
        QVERIFY(!video.setRule(0, fields));
        QCOMPARE(get(projectFile()), before);
    }
    void strictProjectValidation()
    {
        readProject();
        const auto original = QJsonDocument::fromJson(get(projectFile())).object();
        QList<QJsonObject> invalid;
        auto changed = original; changed["schemaVersion"] = 4; invalid.append(changed);
        changed = original; changed["schemaVersion"] = "1"; invalid.append(changed);
        changed = original; changed["unexpected"] = true; invalid.append(changed);
        for (const QString &field : {QStringLiteral("order"), QStringLiteral("untilDayOffset"), QStringLiteral("directory")}) {
            auto rows = original["music"].toArray();
            auto missing = rows[0].toObject(); missing.remove(field); rows[0] = missing;
            changed = original; changed["music"] = rows; invalid.append(changed);
        }
        auto music = original.value("music").toArray();
        auto row = music[0].toObject(); row["volume"] = "70"; music[0] = row;
        changed = original; changed["music"] = music; invalid.append(changed);
        for (const QJsonValue &order : {QJsonValue("random"), QJsonValue(true), QJsonValue(QJsonValue::Null)}) {
            music = original.value("music").toArray(); row = music[0].toObject(); row["order"] = order; music[0] = row;
            changed = original; changed["music"] = music; invalid.append(changed);
        }
        for (const QJsonValue &directory : {QJsonValue(""), QJsonValue(".."), QJsonValue("../outside"),
                 QJsonValue("C:/outside"), QJsonValue("CON"), QJsonValue(true), QJsonValue(QJsonValue::Null)}) {
            music = original["music"].toArray(); row = music[0].toObject(); row["directory"] = directory; music[0] = row;
            changed = original; changed["music"] = music; invalid.append(changed);
        }
        music = original["music"].toArray(); row = music[0].toObject();
        row.remove("directory"); music[0] = row;
        changed = original; changed["music"] = music; invalid.append(changed);
        music = original["music"].toArray(); row = music[0].toObject();
        row["id"] = "00000000-0000-4000-8000-000000000999";
        row["name"] = "Other"; row["directory"] = "one"; music.append(row);
        changed = original; changed["music"] = music; invalid.append(changed);
        music = original.value("music").toArray(); row = music[0].toObject(); row["weekdays"] = "*"; music[0] = row;
        changed = original; changed["music"] = music; invalid.append(changed);
        auto ads = original.value("advert").toArray(); row = ads[0].toObject(); row["preparedMinutes"] = QJsonArray{1, 22, 42}; ads[0] = row;
        changed = original; changed["advert"] = ads; invalid.append(changed);
        row = original.value("video").toArray()[0].toObject(); row["id"] = original.value("music").toArray()[0].toObject().value("id");
        changed = original; changed["video"] = QJsonArray{row}; invalid.append(changed);
        for (const auto &object : invalid) {
            const QByteArray bytes = QJsonDocument(object).toJson(); put(projectFile(), bytes);
            ProjectRepository::Project project; QString error;
            QVERIFY(!ProjectRepository::load(paths(), &project, &error)); QVERIFY(!error.isEmpty());
            QCOMPARE(get(projectFile()), bytes);
        }
    }
    void failedReloadPreservesMemoryAndBlocksMutations()
    {
        ChannelManager manager(MediaBoxManager::MUSIC); QVERIFY(manager.collectChannels());
        const QByteArray good = get(projectFile());
        put(projectFile(), "broken\n");
        QVERIFY(!manager.collectChannels()); QCOMPARE(manager.channelCount(), 1);
        QVERIFY(!manager.createChannel(channelFields("Two")));
        QVERIFY(!manager.setRule(0, channelFields("One", 10)));
        QVERIFY(!manager.deleteChannel(0));
        QCOMPARE(get(projectFile()), QByteArray("broken\n"));
        put(projectFile(), good); QVERIFY(manager.collectChannels());
        QVERIFY2(manager.setRule(0, channelFields("One", 71)), qPrintable(manager.lastError()));
    }
    void loadingMultipleChannelsPreservesMedia()
    {
        auto project = readProject();
        project.music = {ProjectFixture::channel("One", QTime(8, 0), QTime(12, 0), 70),
            ProjectFixture::channel("Two", QTime(12, 0), QTime(16, 0), 80),
            ProjectFixture::channel("Three", QTime(16, 0), QTime(20, 0), 90)};
        put(projectFile(), ProjectRepository::encode(project));
        const QByteArray before = get(projectFile());
        for (const QString &name : {QStringLiteral("One"), QStringLiteral("Two"), QStringLiteral("Three")})
            put(station.filePath("media/music/" + name + "/track.mp3"), "untouched");
        ChannelManager manager(MediaBoxManager::MUSIC); QVERIFY(manager.collectChannels());
        QCOMPARE(manager.channelCount(), 3); QCOMPARE(get(projectFile()), before);
        for (const QString &name : {QStringLiteral("One"), QStringLiteral("Two"), QStringLiteral("Three")}) {
            QCOMPARE(get(station.filePath("media/music/" + name + "/track.mp3")), QByteArray("untouched"));
            QVERIFY(!QFileInfo::exists(station.filePath("media/music/" + name + "/track.mp3.tag")));
        }
    }

    void wholeRuleAndSingleCellPreserveOtherSections()
    {
        ChannelManager music(MediaBoxManager::MUSIC), video(MediaBoxManager::VIDEO);
        QVERIFY(music.collectChannels()); QVERIFY(video.collectChannels());
        AdvertManager advertsManager;
        const auto original = readProject();
        ChannelModel model(&music); QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
        auto fields = channelFields(); fields[1] = QTime(9, 0); fields[2] = QTime(19, 0);
        QVERIFY(model.setRule(0, fields)); QCOMPARE(changes.size(), 1);
        QVERIFY(model.setData(model.index(0, 6), 45, Qt::EditRole));
        QVERIFY(advertsManager.setRule(0, advertFields(15)));
        auto screen = channelFields("Screen", 25);
        QVERIFY(video.setRule(0, screen));
        const auto saved = readProject();
        QCOMPARE(saved.music[0].volume, 45); QCOMPARE(saved.music[0].start, QTime(9, 0));
        QCOMPARE(saved.video[0].volume, 25); QCOMPARE(saved.advert[0].volume, 15);
        QCOMPARE(saved.music[0].stableId, original.music[0].stableId);
        QCOMPARE(saved.advert[0].stableId, original.advert[0].stableId);
        const QByteArray good = get(projectFile()); fields[6] = 101;
        QVERIFY(!model.setRule(0, fields)); QCOMPARE(get(projectFile()), good);
    }
    void preparedPhaseStaysInProjectAndPreviewRole()
    {
        AdvertManager manager; AdvertModel model(&manager);
        QCOMPARE(manager.compiledMinutes(0), QList<int>({2, 22, 42}));
        QVERIFY(model.setRule(0, advertFields(15)));
        QCOMPARE(readProject().advert[0].compiledMinutes, QList<int>({2, 22, 42}));
        QCOMPARE(model.data(model.index(0, 0), AdvertModel::CompiledMinutesRole).toList(), QVariantList({2, 22, 42}));
        const QByteArray saved = get(projectFile()); QVERIFY(model.setRule(0, advertFields(15)));
        QCOMPARE(get(projectFile()), saved);
        QVERIFY(manager.delAdvert(0)); QVERIFY(readProject().advert.isEmpty());
    }
    void duplicateRowsRetainIdentityAndPhase()
    {
        auto project = readProject();
        project.advert.append(ProjectFixture::advert("ad.mp3", "10", "3",
            QDate(2026, 10, 4), QDate(2026, 10, 4), 40, {7, 27, 47}));
        put(projectFile(), ProjectRepository::encode(project));
        AdvertManager manager; const QString survivor = readProject().advert[1].stableId;
        QVERIFY(manager.delAdvert(0));
        QCOMPARE(manager.compiledMinutes(0), QList<int>({7, 27, 47}));
        QCOMPARE(readProject().advert[0].stableId, survivor);
    }

    void newProjectCreatesOnlyCurrentStorage()
    {
        QTemporaryDir fresh;
        ProjectRepository::Paths p{fresh.path(), fresh.filePath("music"), fresh.filePath("video")};
        ProjectRepository::Project project; QString error;
        QVERIFY2(ProjectRepository::load(p, &project, &error), qPrintable(error));
        QVERIFY(project.music.isEmpty()); QVERIFY(project.advert.isEmpty());
        QVERIFY(QFileInfo::exists(fresh.filePath("project.json")));
        QVERIFY(!QFileInfo::exists(fresh.filePath("timetable")));
    }
    void mixedExactAndFrequencyRulesKeepSeparatePhases()
    {
        auto project = readProject();
        project.advert = {ProjectFixture::advert("ad.mp3", "10", "0m,20m,40m",
            QDate(2026, 10, 4), QDate(2026, 10, 4), 70),
            ProjectFixture::advert("ad.mp3", "10", "3",
            QDate(2026, 10, 4), QDate(2026, 10, 4), 70, {7, 27, 47})};
        put(projectFile(), ProjectRepository::encode(project));
        const auto reopened = readProject();
        QVERIFY(reopened.advert[0].compiledMinutes.isEmpty());
        QCOMPARE(reopened.advert[1].compiledMinutes, QList<int>({7, 27, 47}));
    }

    void renamePreservesAdvancedPublicationAndOwnsItsOriginalDirectory()
    {
        const QString track = station.filePath("media/music/One/rename-regression.mp3");
        put(track, "audio");
        ChannelManager manager(MediaBoxManager::MUSIC);
        QVERIFY2(manager.collectChannels(), qPrintable(manager.lastError()));
        const QString channelId = manager.channel(0).ruleId();
        QJsonObject source{{"channels", QJsonArray{QJsonObject{{"id", channelId}, {"name", "One"},
            {"directory", "One"}, {"start", "08:00"}, {"end", "18:00"}, {"untilDayOffset", 0},
            {"weekdays", "*"}, {"days", "*"}, {"months", "*"}, {"volume", 70}, {"order", "sequential"},
            {"paths", QJsonArray{track}}}}}, {"adverts", QJsonArray{}}};
        QTemporaryDir publicationDirectory;
        QVERIFY(publicationDirectory.isValid());
        const QString contentRoot = station.filePath("media");
        const QDateTime now = QDateTime::fromString("2026-10-04T10:00:00Z", Qt::ISODate);
        QJsonObject document;
        QString error;
        bool advanced = false;
        QVERIFY2(SchedulePublication::draft(publicationDirectory.path(), contentRoot, source,
                                           &document, &advanced, &error), qPrintable(error));
        document["timeZone"] = "UTC";
        document["validity"] = QJsonObject{{"from", "2026-10-01"}, {"until", "2027-10-01"}};
        QVERIFY2(SchedulePublication::saveDraft(publicationDirectory.path(), document, &error), qPrintable(error));
        SchedulePublication::Publication publication;
        QVERIFY2(SchedulePublication::publish(publicationDirectory.path(), contentRoot, document,
                                             &publication, &error), qPrintable(error));
        const QString database = publicationDirectory.filePath("runtime.sqlite");
        {
            MediaBox::ScheduleV1Runtime runtime(database);
            QCOMPARE(runtime.accept(publication.bytes, contentRoot, publication.active, now), QString());
            QCOMPARE(QDir::cleanPath(runtime.selectMusic(now).path), QDir::cleanPath(track));
            QVERIFY2(manager.setRule(0, channelFields("Renamed")), qPrintable(manager.lastError()));
            QCOMPARE(manager.channel(0).channelName(), QStringLiteral("Renamed"));
            QCOMPARE(manager.channel(0).storageDirectory(), QStringLiteral("One"));
            QCOMPARE(manager.channel(0).ruleId(), channelId);
            QCOMPARE(QDir::cleanPath(manager.channel(0).mediaManager().getDirMediaFiles().absolutePath()),
                     QDir::cleanPath(station.filePath("media/music/One")));
            QCOMPARE(get(track), QByteArray("audio"));
            QVERIFY(!QFileInfo::exists(station.filePath("media/music/Renamed")));
            QCOMPARE(QDir::cleanPath(runtime.selectMusic(now).path), QDir::cleanPath(track));
        }
        auto renamed = source["channels"].toArray().first().toObject();
        renamed["name"] = "Renamed"; source["channels"] = QJsonArray{renamed};
        QJsonObject saved;
        QVERIFY2(SchedulePublication::draft(publicationDirectory.path(), contentRoot, source,
                                           &saved, &advanced, &error), qPrintable(error));
        QVERIFY(advanced);
        QCOMPARE(saved["assets"], document["assets"]);
        const auto previousPlaylist = document["playlists"].toArray().first().toObject();
        const auto renamedPlaylist = saved["playlists"].toArray().first().toObject();
        QCOMPARE(renamedPlaylist.value("name"), QJsonValue("Renamed"));
        QCOMPARE(renamedPlaylist.value("id"), previousPlaylist.value("id"));
        QCOMPARE(renamedPlaylist.value("entries"), previousPlaylist.value("entries"));
        QCOMPARE(renamedPlaylist.value("revision"), previousPlaylist.value("revision"));
        SchedulePublication::Publication updated;
        QVERIFY2(SchedulePublication::publish(publicationDirectory.path(), contentRoot, saved,
                                             &updated, &error), qPrintable(error));
        QCOMPARE(updated.active.value("revision").toInt(), publication.active.value("revision").toInt() + 1);
        QCOMPARE(get(publicationDirectory.filePath(publication.active.value("snapshotPath").toString())), publication.bytes);
        {
            MediaBox::ScheduleV1Runtime restored(database);
            QCOMPARE(restored.restore(), QString());
            QCOMPARE(QDir::cleanPath(restored.selectMusic(now).path), QDir::cleanPath(track));
            QCOMPARE(restored.accept(updated.bytes, contentRoot, updated.active, now), QString());
            QCOMPARE(QDir::cleanPath(restored.selectMusic(now).path), QDir::cleanPath(track));
        }
        QVERIFY2(manager.collectChannels(), qPrintable(manager.lastError()));
        QCOMPARE(manager.channel(0).channelName(), QStringLiteral("Renamed"));
        QCOMPARE(manager.channel(0).storageDirectory(), QStringLiteral("One"));
        QVERIFY2(manager.createChannel(channelFields("One")), qPrintable(manager.lastError()));
        QCOMPARE(manager.channelCount(), 2);
        const QString replacementDirectory = manager.channel(1).storageDirectory();
        QVERIFY(replacementDirectory.compare(QStringLiteral("One"), Qt::CaseInsensitive) != 0);
        const QString replacementTrack = manager.channel(1).mediaManager().getDirMediaFiles().filePath("replacement.mp3");
        put(replacementTrack, "replacement");
        QVERIFY2(manager.deleteChannel(0), qPrintable(manager.lastError()));
        QVERIFY(!QFileInfo::exists(track));
        QCOMPARE(get(replacementTrack), QByteArray("replacement"));
        QVERIFY2(manager.collectChannels(), qPrintable(manager.lastError()));
        QCOMPARE(manager.channelCount(), 1);
        QCOMPARE(manager.channel(0).channelName(), QStringLiteral("One"));
        QCOMPARE(manager.channel(0).storageDirectory(), replacementDirectory);
        QCOMPARE(get(replacementTrack), QByteArray("replacement"));
    }
    void directoryJournalRollsBackOrCompletes()
    {
        auto project = readProject(); const QByteArray before = get(projectFile());
        project.music[0].name = "Renamed"; project.music[0].storageDirectory = "Renamed";
        const QByteArray after = ProjectRepository::encode(project);
        put(station.filePath("media/music/One/recovery.mp3"), "audio");
        const QJsonObject journal{{"version", 1}, {"section", "music"}, {"from", "One"}, {"to", "Renamed"},
                {"before", QString::fromLatin1(before.toBase64())}, {"after", QString::fromLatin1(after.toBase64())}};
        QString error;
        QVERIFY(QDir(paths().musicDirectory).rename("One", "Renamed"));
        put(projectFile() + ".pending", QJsonDocument(journal).toJson());
        ProjectRepository::Project restored;
        QVERIFY2(ProjectRepository::load(paths(), &restored, &error), qPrintable(error));
        QCOMPARE(get(station.filePath("media/music/One/recovery.mp3")), QByteArray("audio"));
        QVERIFY(QDir(paths().musicDirectory).rename("One", "Renamed")); put(projectFile(), after);
        put(projectFile() + ".pending", QJsonDocument(journal).toJson());
        QVERIFY2(ProjectRepository::load(paths(), &restored, &error), qPrintable(error));
        QCOMPARE(get(station.filePath("media/music/Renamed/recovery.mp3")), QByteArray("audio"));
        QVERIFY(ProjectRepository::load(paths(), &restored, &error));
        QVERIFY(QDir(paths().musicDirectory).rename("Renamed", "One"));
    }
    void actualReplaceFailurePreservesProjectMemoryAndMedia()
    {
#ifdef Q_OS_WIN
        ChannelManager channels(MediaBoxManager::MUSIC); QVERIFY(channels.collectChannels());
        ChannelModel channelModel(&channels);
        AdvertManager ads; AdvertModel advertModel(&ads);
        const QByteArray before = get(projectFile());
        {
            DenyReplacement held(projectFile()); QVERIFY(held.valid());
            QVERIFY(!channelModel.setRule(0, channelFields("One", 10)));
            QCOMPARE(get(projectFile()), before); QCOMPARE(channels.channel(0).volume(), 70);
            QVERIFY(!channelModel.lastError().isEmpty());
            QVERIFY(!channelModel.setRule(0, channelFields("Renamed", 10)));
            QVERIFY(QFileInfo::exists(station.filePath("media/music/One")));
            QVERIFY(!QFileInfo::exists(station.filePath("media/music/Renamed")));
            QVERIFY(!advertModel.setRule(0, advertFields(10)));
            QCOMPARE(get(projectFile()), before); QCOMPARE(ads.advert(0).volume(), 70);
            QVERIFY(!advertModel.lastError().isEmpty());
            QVERIFY(!channels.deleteChannel(0));
            QVERIFY(QFileInfo::exists(station.filePath("media/music/One")));
        }
        QVERIFY(advertModel.setRule(0, advertFields(10)));
#else
        QSKIP("Windows replacement-denial regression uses native file sharing");
#endif
    }
};
QTEST_GUILESS_MAIN(SchedulePersistenceTests)
#include "tst_schedulepersistence.moc"
