#include "mediaimportservice.h"
#include "settings.h"
#include "mediamanager.h"
#include "mediamodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QtEndian>

namespace {
bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

bool writeWave(const QString &path, int seconds)
{
    // Valid mono 8-bit PCM: duration is read from the media, not its sidecar.
    const quint32 bytes = seconds * 8000;
    QByteArray header("RIFF");
    const auto append32 = [&header](quint32 value) {
        const auto little = qToLittleEndian(value);
        header.append(reinterpret_cast<const char *>(&little), sizeof(little));
    };
    const auto append16 = [&header](quint16 value) {
        const auto little = qToLittleEndian(value);
        header.append(reinterpret_cast<const char *>(&little), sizeof(little));
    };
    append32(bytes + 36);
    header += "WAVEfmt ";
    append32(16);
    append16(1);
    append16(1);
    append32(8000);
    append32(8000);
    append16(1);
    append16(8);
    header += "data";
    append32(bytes);
    return writeFile(path, header + QByteArray(bytes, char(128)));
}

MediaImportRequest requestFor(const QString &target, const QStringList &sources)
{
    MediaImportRequest request;
    request.targetDirectory = target;
    request.paths = sources;
    request.acceptedFormats = {QStringLiteral("*.wav")};
    request.libraryFormats = request.acceptedFormats;
    return request;
}
}

class MediaImportTest final : public QObject
{
    Q_OBJECT
    QTemporaryDir m_fixture;
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_fixture.isValid());
        qApp->setProperty("restylePreviewSettings", m_fixture.filePath("manager.conf"));
        qApp->setProperty("restylePreviewStation", m_fixture.path());
        QVERIFY(writeFile(m_fixture.filePath("mediabox.conf"),
            "[mediastation]\nmediabox_id=-1\nmediabox_name=ImportTest\nmedia=media\n"));
        QVERIFY(writeFile(m_fixture.filePath("project.json"),
            "{\"format\":\"mediabox.manager-project\",\"schemaVersion\":3,\"music\":[],\"video\":[],\"advert\":[]}\n"));
        for (const QString &directory : {"media/music", "media/video", "media/ads"})
            QVERIFY(QDir().mkpath(m_fixture.filePath(directory)));
        QSettings settings(m_fixture.filePath("manager.conf"), QSettings::IniFormat);
        settings.setValue("FileFormats/AudioFormats/wav", true);
        settings.setValue("FileFormats/VideoFormats/mp4", true);
        settings.sync();
        QCOMPARE(settings.status(), QSettings::NoError);
        Settings isolatedSettings;
        QCOMPARE(isolatedSettings.availablelAudioFileFormats(), QStringList{"*.wav"});
        QVERIFY(StationManager::Instance().update());
        QCOMPARE(StationManager::Instance().get(), m_fixture.path());
    }

    void scanningDoesNotWriteTagsOrRemoveLongExistingFiles()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(writeWave(directory.filePath("long.wav"), 301));
        QVERIFY(writeWave(directory.filePath("uncached.wav"), 1));
        const QString sidecar = directory.filePath("long.wav.tag");
        const QByteArray cache("[General]\nlength=901\ntitle=Existing title\n");
        QVERIFY(writeFile(sidecar, cache));
        const auto before = QDir(directory.path()).entryList(QDir::Files);
        const auto snapshot = MediaImportService::scanDirectory(directory.path(), {"*.wav"});
        QCOMPARE(snapshot.size(), 2);
        QCOMPARE(snapshot.first().length(), uint(301));
        QVERIFY(snapshot.first().title().isEmpty());
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files), before);
        QCOMPARE(readFile(sidecar), cache);
        QVERIFY(!QFileInfo::exists(directory.filePath("uncached.wav.tag")));
    }

    void asyncImportPublishesOneSnapshotInTheModelThread()
    {
        QTemporaryDir source;
        QTemporaryDir target;
        QVERIFY(source.isValid() && target.isValid());
        for (int i = 0; i < 5; ++i)
            QVERIFY(writeWave(source.filePath(QString::number(i) + ".wav"), 2));
        MediaManager manager(target.path(), MUSIC);
        MediaModel model(&manager, MUSIC);
        MediaImportService service;
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        QSignalSpy finished(&service, &MediaImportService::finished);
        QSignalSpy progress(&service, &MediaImportService::progress);
        bool guiThread = false;
        connect(&service, &MediaImportService::finished, &model, [&](const MediaImportResult &result) {
            guiThread = QThread::currentThread() == model.thread();
            manager.applySnapshot(result.snapshot);
        });
        QVERIFY(service.start(requestFor(target.path(), {source.path()})));
        QVERIFY(service.isRunning());
        QCOMPARE(reset.size(), 0);
        QCOMPARE(finished.size(), 0);
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(guiThread);
        QCOMPARE(reset.size(), 1);
        QCOMPARE(model.rowCount(), 5);
        QVERIFY(!progress.isEmpty());
        QCOMPARE(finished.first().first().value<MediaImportResult>().imported, 5);
    }

    void cancellationInsideAFileRetainsCompletedFilesAndRemovesOnlyTemporaryFile()
    {
        QTemporaryDir source;
        QTemporaryDir target;
        QVERIFY(source.isValid() && target.isValid());
        QVERIFY(writeWave(source.filePath("small.wav"), 1));
        QVERIFY(writeWave(source.filePath("large.wav"), 200));
        QVERIFY(writeWave(target.filePath("existing.wav"), 1));
        const auto original = readFile(target.filePath("existing.wav"));
        auto flag = std::make_shared<std::atomic_bool>(false);
        const auto result = MediaImportService::run(
            requestFor(target.path(), {source.filePath("small.wav"), source.filePath("large.wav")}), flag,
            [flag](int, int, const QString &file, qint64 copied, qint64) {
                if (file == "large.wav" && copied > 0)
                    flag->store(true);
            });
        QVERIFY(result.cancelled);
        QCOMPARE(result.imported, 1);
        QCOMPARE(result.snapshot.size(), 2);
        QVERIFY(QFileInfo::exists(target.filePath("small.wav")));
        QVERIFY(!QFileInfo::exists(target.filePath("large.wav")));
        QCOMPARE(readFile(target.filePath("existing.wav")), original);
        QCOMPARE(QDir(target.path()).entryList(QDir::Files | QDir::Hidden).size(), 2);
        QVERIFY(QFileInfo::exists(source.filePath("large.wav")));
    }

    void collisionsNeverOverwriteExistingFiles()
    {
        QTemporaryDir source;
        QTemporaryDir target;
        QVERIFY(source.isValid() && target.isValid());
        QVERIFY(QDir().mkpath(source.filePath("other")));
        QVERIFY(writeWave(source.filePath("same.wav"), 1));
        QVERIFY(writeWave(source.filePath("other/same.wav"), 2));
        const auto result = MediaImportService::run(requestFor(target.path(),
            {source.filePath("same.wav"), source.filePath("other/same.wav")}));
        QCOMPARE(result.imported, 1);
        QCOMPARE(result.errors.size(), 1);
        QCOMPARE(readFile(target.filePath("same.wav")), readFile(source.filePath("same.wav")));
        const auto again = MediaImportService::run(requestFor(target.path(), {source.filePath("other/same.wav")}));
        QCOMPARE(again.imported, 0);
        QCOMPARE(readFile(target.filePath("same.wav")), readFile(source.filePath("same.wav")));
    }

    void disappearedSourceReportsFailureWithoutLeavingAPartialFile()
    {
        QTemporaryDir source;
        QTemporaryDir target;
        QVERIFY(source.isValid() && target.isValid());
        const QString input = source.filePath("input.wav");
        const QString movedInput = source.filePath("input.moved");
        QVERIFY(writeWave(input, 1));
        QVERIFY(writeWave(target.filePath("existing.wav"), 1));
        const QByteArray existing = readFile(target.filePath("existing.wav"));
        bool moved = false;
        const auto result = MediaImportService::run(requestFor(target.path(), {input}), {},
            [&](int, int total, const QString &file, qint64 copied, qint64) {
                // Deterministic open failure after discovery, independent of
                // filesystem ACLs and whether the test runs as administrator.
                if (total > 0 && file == "input.wav" && copied == 0 && !moved)
                    moved = QFile::rename(input, movedInput);
            });
        QVERIFY(moved);
        QVERIFY(!result.cancelled);
        QCOMPARE(result.imported, 0);
        QCOMPARE(result.errors.size(), 1);
        QCOMPARE(result.snapshot.size(), 1);
        QCOMPARE(readFile(target.filePath("existing.wav")), existing);
        QVERIFY(!QFileInfo::exists(target.filePath("input.wav")));
        QVERIFY(QDir(target.path()).entryList({".mediabox-import-*"}, QDir::Files | QDir::Hidden).isEmpty());
        QVERIFY(QFileInfo::exists(movedInput));
    }

    void invalidDestinationPreservesFilesAndPreviousSnapshot()
    {
        QTemporaryDir source;
        QTemporaryDir target;
        QVERIFY(source.isValid() && target.isValid());
        QVERIFY(writeWave(source.filePath("input.wav"), 1));
        const QString obstruction = target.filePath("library");
        const QByteArray original("Existing file must not be overwritten");
        QVERIFY(writeFile(obstruction, original));
        auto request = requestFor(obstruction, {source.filePath("input.wav")});
        request.initialSnapshot = MediaImportService::scanDirectory(source.path(), {"*.wav"});
        const auto result = MediaImportService::run(request);
        QVERIFY(!result.cancelled);
        QCOMPARE(result.imported, 0);
        QCOMPARE(result.errors.size(), 1);
        QCOMPARE(result.snapshot.size(), 1);
        QCOMPARE(readFile(obstruction), original);
        QCOMPARE(QDir(target.path()).entryList(QDir::Files | QDir::Hidden).size(), 1);
        QVERIFY(QFileInfo::exists(source.filePath("input.wav")));
    }

    void trialLimitsRejectBeforeCopyWithoutRemovingExistingFiles()
    {
        QTemporaryDir source;
        QTemporaryDir target;
        QVERIFY(source.isValid() && target.isValid());
        QVERIFY(writeWave(source.filePath("long.wav"), 301));
        QVERIFY(writeWave(source.filePath("short.wav"), 1));
        auto request = requestFor(target.path(), {source.filePath("long.wav"), source.filePath("short.wav")});
        request.maximumDurationSeconds = 300;
        request.maximumFiles = 1;
        auto result = MediaImportService::run(request);
        QCOMPARE(result.imported, 1);
        QCOMPARE(result.errors.size(), 1);
        QVERIFY(!QFileInfo::exists(target.filePath("long.wav")));
        QVERIFY(QFileInfo::exists(target.filePath("short.wav")));
        QVERIFY(writeWave(target.filePath("already-long.wav"), 301));
        result = MediaImportService::run(request);
        QCOMPARE(result.imported, 0);
        QCOMPARE(result.snapshot.size(), 2);
        QVERIFY(QFileInfo::exists(target.filePath("already-long.wav")));
    }

    void cancelledScanPreservesPreviousCompleteSnapshot()
    {
        QTemporaryDir target;
        QVERIFY(writeWave(target.filePath("one.wav"), 1));
        auto request = requestFor(target.path(), {});
        request.initialSnapshot = MediaImportService::scanDirectory(target.path(), {"*.wav"});
        auto flag = std::make_shared<std::atomic_bool>(true);
        const auto result = MediaImportService::run(request, flag);
        QVERIFY(result.cancelled);
        QCOMPARE(result.snapshot.size(), 1);
        QCOMPARE(result.snapshot.first().fileName(), QStringLiteral("one.wav"));
    }

    void destroyingAnActiveServiceCancelsAndJoinsItsWorker()
    {
        QTemporaryDir source;
        QTemporaryDir target;
        QVERIFY(writeWave(source.filePath("large.wav"), 100));
        auto *service = new MediaImportService;
        QVERIFY(service->start(requestFor(target.path(), {source.filePath("large.wav")})));
        delete service;
        QVERIFY(QFileInfo::exists(source.filePath("large.wav")));
        QVERIFY(QDir(target.path()).entryList({".mediabox-import-*"}, QDir::Files | QDir::Hidden).isEmpty());
    }
};

QTEST_GUILESS_MAIN(MediaImportTest)
#include "tst_mediaimport.moc"
