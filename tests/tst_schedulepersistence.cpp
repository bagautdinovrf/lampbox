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
    QString timetable() const { return station.filePath("timetable/timetable"); }
    QString adverts() const { return station.filePath("timetable/advertView"); }
    QString derived() const { return station.filePath("timetable/adverttable"); }
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
        put(timetable(), "One 08:00 18:00 * * * 70\n");
        put(station.filePath("timetable/vtimetable"), "Screen 00:00 00:00 * * * 100\n");
        put(adverts(), "ad.mp3;10;3;*;04.10.2026;04.10.2026;70\n");
        put(derived(), "ad.mp3;10;2m,22m,42m;*;04.10.2026;04.10.2026;70;0\n");
        QDir().mkpath(station.filePath("media/music/One"));
        QDir().mkpath(station.filePath("media/video/Screen"));
    }
    void malformedImport_data()
    {
        QTest::addColumn<QByteArray>("input");
        QTest::newRow("blank") << QByteArray("\n");
        QTest::newRow("truncated") << QByteArray("One 08:00\n");
        QTest::newRow("extra-field") << QByteArray("One 08:00 18:00 * * * 70 extra\n");
        QTest::newRow("bad-time") << QByteArray("One 24:00 18:00 * * * 70\n");
        QTest::newRow("path") << QByteArray("../other 08:00 18:00 * * * 70\n");
        QTest::newRow("duplicate") << QByteArray("One 08:00 18:00 * * * 70\none 08:00 18:00 * * * 70\n");
        QTest::newRow("utf8") << QByteArray("\xff\xfe\n");
    }
    void malformedImport()
    {
        QFETCH(QByteArray, input);
        put(timetable(), input);
        ProjectRepository::Project project;
        QString error;
        QVERIFY(!ProjectRepository::load(paths(), &project, &error));
        QVERIFY(!error.isEmpty()); QVERIFY(!QFileInfo::exists(projectFile()));
        QCOMPARE(get(timetable()), input);
        ChannelManager manager(MediaBoxManager::MUSIC);
        QVERIFY(!manager.collectChannels());
        QVERIFY(!manager.createChannel(channelFields("Two")));
        QVERIFY(!QFileInfo::exists(projectFile()));
        QCOMPARE(get(timetable()), input);
    }
    void importsAllSectionsOnceAndRetainsOriginals()
    {
        const QByteArray music = get(timetable()), source = get(adverts()), generated = get(derived());
        const auto project = readProject();
        QCOMPARE(project.music.size(), 1); QCOMPARE(project.video.size(), 1); QCOMPARE(project.advert.size(), 1);
        QVERIFY(!project.music[0].stableId.isEmpty());
        QVERIFY(project.music[0].stableId != project.video[0].stableId);
        QCOMPARE(project.advert[0].compiledMinutes, QList<int>({2, 22, 42}));
        const auto object = QJsonDocument::fromJson(get(projectFile())).object();
        QCOMPARE(object.value("format").toString(), QStringLiteral("mediabox.manager-project"));
        QVERIFY(object.value("music").toArray()[0].toObject().value("weekdays").isArray());
        const QByteArray imported = get(projectFile());
        QCOMPARE(get(timetable()), music); QCOMPARE(get(adverts()), source); QCOMPARE(get(derived()), generated);
        put(timetable(), "broken after successful import\n");
        const auto again = readProject();
        QCOMPARE(again.music[0].stableId, project.music[0].stableId);
        QCOMPARE(get(projectFile()), imported);
    }
    void failedWholeImportCanRetryWithoutPartialProject()
    {
        const QByteArray broken("ad.mp3;10;3\n");
        put(adverts(), broken);
        AdvertManager manager;
        QVERIFY(!manager.lastError().isEmpty()); QVERIFY(!manager.addAdvert(advertFields()));
        QVERIFY(!QFileInfo::exists(projectFile())); QCOMPARE(get(adverts()), broken);
        put(adverts(), "ad.mp3;10;3;*;04.10.2026;04.10.2026;70\n");
        QVERIFY(manager.collectAdvert()); QVERIFY(QFileInfo::exists(projectFile()));
        QCOMPARE(manager.count(), 1);
    }
    void channelOrderDefaultsAndSurvivesEditsAndReload()
    {
        auto original = readProject();
        QCOMPARE(original.music[0].order, QStringLiteral("shuffle_cycle"));
        QCOMPARE(original.video[0].order, QStringLiteral("shuffle_cycle"));
        auto legacy = QJsonDocument::fromJson(get(projectFile())).object();
        for (const QString &section : {QStringLiteral("music"), QStringLiteral("video")}) {
            auto rows = legacy.value(section).toArray();
            auto row = rows[0].toObject(); row.remove("order"); rows[0] = row;
            legacy[section] = rows;
        }
        put(projectFile(), QJsonDocument(legacy).toJson());
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
        auto changed = original; changed["schemaVersion"] = 2; invalid.append(changed);
        changed = original; changed["schemaVersion"] = "1"; invalid.append(changed);
        changed = original; changed["unexpected"] = true; invalid.append(changed);
        auto music = original.value("music").toArray();
        auto row = music[0].toObject(); row["volume"] = "70"; music[0] = row;
        changed = original; changed["music"] = music; invalid.append(changed);
        for (const QJsonValue &order : {QJsonValue("random"), QJsonValue(true), QJsonValue(QJsonValue::Null)}) {
            music = original.value("music").toArray(); row = music[0].toObject(); row["order"] = order; music[0] = row;
            changed = original; changed["music"] = music; invalid.append(changed);
        }
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
        QVERIFY(manager.setRule(0, channelFields("One", 71)));
    }
    void importingMultipleChannelsPreservesMedia()
    {
        const QByteArray input("One 08:00 12:00 * * * 70\nTwo 12:00 16:00 * * * 80\nThree 16:00 20:00 * * * 90\n");
        put(timetable(), input);
        for (const QString &name : {QStringLiteral("One"), QStringLiteral("Two"), QStringLiteral("Three")})
            put(station.filePath("media/music/" + name + "/track.mp3"), "untouched");
        ChannelManager manager(MediaBoxManager::MUSIC); QVERIFY(manager.collectChannels());
        QCOMPARE(manager.channelCount(), 3); QCOMPARE(get(timetable()), input);
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
    void legacyPhaseStaysInProjectAndPreviewRole()
    {
        const QByteArray source = get(adverts()), generated = get(derived());
        AdvertManager manager; AdvertModel model(&manager);
        QCOMPARE(manager.compiledMinutes(0), QList<int>({2, 22, 42}));
        QVERIFY(model.setRule(0, advertFields(15)));
        QCOMPARE(readProject().advert[0].compiledMinutes, QList<int>({2, 22, 42}));
        QCOMPARE(model.data(model.index(0, 0), AdvertModel::CompiledMinutesRole).toList(), QVariantList({2, 22, 42}));
        const QByteArray saved = get(projectFile()); QVERIFY(model.setRule(0, advertFields(15)));
        QCOMPARE(get(projectFile()), saved);
        QCOMPARE(get(adverts()), source); QCOMPARE(get(derived()), generated);
        QVERIFY(manager.delAdvert(0)); QVERIFY(readProject().advert.isEmpty());
        QCOMPARE(get(adverts()), source); QCOMPARE(get(derived()), generated);
    }
    void duplicateRowsRetainIdentityAndPhase()
    {
        put(adverts(), "ad.mp3;10;3;*;04.10.2026;04.10.2026;70\nad.mp3;10;3;*;04.10.2026;04.10.2026;40\n");
        put(derived(), "ad.mp3;10;2m,22m,42m;*;04.10.2026;04.10.2026;70;0\nad.mp3;10;7m,27m,47m;*;04.10.2026;04.10.2026;40;0\n");
        AdvertManager manager; const QString survivor = readProject().advert[1].stableId;
        QVERIFY(manager.delAdvert(0));
        QCOMPARE(manager.compiledMinutes(0), QList<int>({7, 27, 47}));
        QCOMPARE(readProject().advert[0].stableId, survivor);
    }
    void newProjectCreatesNoLegacyFiles()
    {
        QTemporaryDir fresh;
        ProjectRepository::Paths p{fresh.path(), fresh.filePath("music"), fresh.filePath("video")};
        ProjectRepository::Project project; QString error;
        QVERIFY2(ProjectRepository::load(p, &project, &error), qPrintable(error));
        QVERIFY(project.music.isEmpty()); QVERIFY(project.advert.isEmpty());
        QVERIFY(QFileInfo::exists(fresh.filePath("project.json")));
        QVERIFY(!QFileInfo::exists(fresh.filePath("timetable")));
    }
    void mixedExactAndFrequencyImportKeepSeparatePhases()
    {
        put(adverts(), "ad.mp3;10;0m,20m,40m;*;04.10.2026;04.10.2026;70\nad.mp3;10;3;*;04.10.2026;04.10.2026;70\n");
        put(derived(), "ad.mp3;10;0m,20m,40m;*;04.10.2026;04.10.2026;70;0\nad.mp3;10;7m,27m,47m;*;04.10.2026;04.10.2026;70;0\n");
        const auto project = readProject();
        QVERIFY(project.advert[0].compiledMinutes.isEmpty());
        QCOMPARE(project.advert[1].compiledMinutes, QList<int>({7, 27, 47}));
    }
    void oldNetworkViewAcceptsGeneratedColumnsAndDisabledRows()
    {
        const QByteArray oldView("ad.mp3;10;2m,22m,42m;*;04.10.2026;04.10.2026;70;0\ndisabled.mp3;10;*;*;04.10.2026;04.10.2026;70;0\n");
        put(adverts(), oldView); put(derived(), oldView);
        const auto project = readProject();
        QCOMPARE(project.advert.size(), 2);
        QCOMPARE(project.advert[0].timing, QStringLiteral("2m,22m,42m"));
        QCOMPARE(project.advert[1].timing, QStringLiteral("*"));
        QCOMPARE(get(adverts()), oldView);
    }
    void unsupportedLegacyOrderBlocksLossyImport()
    {
        put(derived(), "ad.mp3;10;2m,22m,42m;*;04.10.2026;04.10.2026;70;3\n");
        ProjectRepository::Project project; QString error;
        QVERIFY(!ProjectRepository::load(paths(), &project, &error));
        QVERIFY(!QFileInfo::exists(projectFile()));
        QVERIFY(!error.isEmpty());
    }
    void directoryJournalRollsBackOrCompletes()
    {
        auto project = readProject(); const QByteArray before = get(projectFile());
        project.music[0].name = "Renamed"; const QByteArray after = ProjectRepository::encode(project);
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
        const QByteArray before = get(projectFile()), oldLegacy = get(timetable());
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
        QCOMPARE(get(timetable()), oldLegacy);
#else
        QSKIP("Windows replacement-denial regression uses native file sharing");
#endif
    }
};
QTEST_GUILESS_MAIN(SchedulePersistenceTests)
#include "tst_schedulepersistence.moc"
