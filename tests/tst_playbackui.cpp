#include "advertmanager.h"
#include "audiobackend.h"
#include "channelmanager.h"
#include "controlserver.h"
#include "mainwindow.h"
#include "mediacontroller.h"
#include "mediaimportservice.h"
#include "mediamodel.h"
#include "playerengine.h"
#include "restyletheme.h"
#include "settings.h"
#include "stationmanager.h"
#include "videocontroller.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCheckBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QRawFont>
#include <QScreen>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeView>
#include <QUuid>

namespace {
QString fixtureRoot;

void put(const QString &path, const QByteArray &contents = {})
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        qFatal("Cannot create isolated playback fixture directory");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size())
        qFatal("Cannot write isolated playback fixture");
}

void mediaFile(const QString &path)
{
    put(path, QByteArray(1024, '\0'));
    QSettings tags(path + ".tag", QSettings::IniFormat);
    tags.setValue("title", QFileInfo(path).completeBaseName());
    tags.setValue("artist", "Тестовая студия");
    tags.setValue("length", 120);
    tags.sync();
}

QStringList filesFor(const QString &kind, const QString &channel)
{
    const QDir directory(fixtureRoot + "/media/" + kind + '/' + channel);
    QStringList result;
    const QString pattern = kind == "music" ? "*.mp3" : "*.mp4";
    for (const auto &file : directory.entryInfoList({pattern}, QDir::Files, QDir::Name))
        result.append(file.absoluteFilePath());
    return result;
}

QString token() { return QString(64, QLatin1Char('a')); }

class Backend final : public MediaBox::AudioBackend
{
public:
    void setSource(const QUrl &source) override { current = source; }
    void play() override { emit stateChanged(State::Playing); }
    void pause() override { emit stateChanged(State::Paused); }
    void stop() override { emit stateChanged(State::Stopped); }
    void seek(qint64 value) override { emit positionChanged(value); }
    void setVolume(int value) override { volume = value; }
    void setMuted(bool) override {}
    void finish() { emit finished(); }
    QUrl current;
    int volume = 100;
};

QJsonObject videoStatus(const QJsonObject &playback)
{
    return {{"application", "MediaBoxVPlayer"},
            {"displays", QJsonArray{QJsonObject{{"id", "display-a"}, {"name", "Тестовый монитор"}, {"index", 0}}}},
            {"windows", QJsonArray{QJsonObject{{"id", "screen-a"}, {"name", "Экран"},
                {"screen", "display-a"}, {"actualScreen", "display-a"}, {"fullscreen", false},
                {"playback", playback}}}}, {"persistenceError", ""}};
}

void settle()
{
    QApplication::sendPostedEvents();
    QApplication::processEvents();
    QTest::qWait(30);
}

bool capture(MainWindow &window, const QString &name)
{
    const QString directory = qEnvironmentVariable("PLAYBACK_UI_CAPTURE_DIR");
    if (directory.isEmpty()) return true;
    if (!QDir().mkpath(directory)) return false;
    const QSize previousSize = window.size();
    window.resize(1440, 900);
    settle();
    const bool saved = window.grab().save(QDir(directory).absoluteFilePath(name + ".png"));
    window.resize(previousSize);
    return saved;
}

QStringList playingNames(QAbstractItemModel *model)
{
    QStringList names;
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto index = model->index(row, 0);
        if (index.data(MediaModel::PlayingRole).toBool())
            names.append(index.data(MediaModel::FileNameRole).toString());
    }
    names.sort();
    return names;
}
} // namespace

class FileUrlReceiver final : public QObject
{
    Q_OBJECT
public:
    QList<QUrl> urls;
public slots:
    void receive(const QUrl &url) { urls.append(url); }
};

class PlaybackUiTests final : public QObject
{
    Q_OBJECT

    void seed(MainWindow &window, const QString &order = QStringLiteral("shuffle_cycle"))
    {
        for (int page = MainWindow::PAGE_MUSIC; page <= MainWindow::PAGE_VIDEO; ++page) {
            auto *manager = window.mChannelManagers[page];
            const QStringList names = page == MainWindow::PAGE_MUSIC
                ? QStringList{"Дневной_канал", "Ручной_канал"} : QStringList{"Природа", "Город"};
            for (int row = 0; row < names.size(); ++row) {
                window.mPages[page].source->setMediaManager(nullptr);
                // Disjoint annual calendars: the second channel is inactive in
                // October, but remains manually playable and valid in January.
                const QString months = row == 0 ? "2,3,4,5,6,7,8,9,10,11,12" : "1";
                const int volume = row == 0 ? 21 : page == MainWindow::PAGE_MUSIC ? 63 : 78;
                QVERIFY2(manager->createChannel({names[row], QTime(0, 0), QTime(23, 59),
                    "*", "*", months, volume, order}), qPrintable(manager->lastError()));
                const QString kind = page == MainWindow::PAGE_MUSIC ? "music" : "video";
                const QString extension = page == MainWindow::PAGE_MUSIC ? ".mp3" : ".mp4";
                const QString folder = fixtureRoot + "/media/" + kind + '/' + names[row];
                for (const QString &name : {QStringLiteral("Первый"), QStringLiteral("Второй"), QStringLiteral("Третий")})
                    mediaFile(folder + '/' + name + extension);
                manager->channel(row).mediaManager().collectMediaFiles();
            }
            window.updatePage(page);
            window.mPages[page].channels->selectRow(1);
            window.selectChannel(page, 1);
        }
        window.mScheduleUpdateTimer->stop();
    }

    void injectAudio(MainWindow &window, quint16 port)
    {
        window.mMediaController = new MediaController(&window);
        window.mMediaController->setTiming({10000, 500, 1000, 1000, 1000});
        connect(window.mMediaController, &MediaBoxPlayerClient::connectionStateChanged,
                &window, &MainWindow::updatePlayerState);
        connect(window.mMediaController, &MediaBoxPlayerClient::statusChanged,
                &window, &MainWindow::updatePlayerState);
        // Normal controller connection suppresses its deferred default connection.
        window.mMediaController->connectToPlayer({"127.0.0.1", port, token()});
    }

    void disconnectControllers(MainWindow &window)
    {
        if (window.mMediaController) window.mMediaController->disconnectFromPlayer();
        if (window.mVideoController) window.mVideoController->disconnectFromPlayer();
    }

private slots:
    void init()
    {
        qApp->setProperty("restylePreviewStation", fixtureRoot);
        QFile::remove(fixtureRoot + "/project.json");
        QFile::remove(fixtureRoot + "/project.json.pending");
        QFile::remove(fixtureRoot + "/schedule-project.json");
        QFile::remove(fixtureRoot + "/active.json");
        QDir(fixtureRoot + "/media/music").removeRecursively();
        QDir(fixtureRoot + "/media/video").removeRecursively();
        put(fixtureRoot + "/timetable/timetable");
        put(fixtureRoot + "/timetable/vtimetable");
        put(fixtureRoot + "/timetable/advertView", QStringLiteral(
            "Объявление.mp3;10;00m,30m;*;01.10.2026;31.10.2026;75\n"
            "Кофе.mp3;10;3;*;01.10.2026;31.10.2026;70\n").toUtf8());
        put(fixtureRoot + "/timetable/adverttable", QStringLiteral(
            "Кофе.mp3;10;2m,22m,42m;*;01.10.2026;31.10.2026;70;0\n").toUtf8());
        mediaFile(fixtureRoot + "/media/ads/Объявление.mp3");
        mediaFile(fixtureRoot + "/media/ads/Кофе.mp3");
        QVERIFY(Settings().setMainWindowGeometry({}));
        QVERIFY(Settings().setVideoWindowProfiles({}));
        QVERIFY(StationManager::Instance().update());
        QCOMPARE(StationManager::Instance().get(), fixtureRoot);
    }

    void cleanup()
    {
        QDesktopServices::unsetUrlHandler("file");
        qApp->setProperty("restylePreviewStation", fixtureRoot);
        QVERIFY(StationManager::Instance().update());
    }

    void selectedAudioChannelAndAtomicSchedule_data()
    {
        QTest::addColumn<QString>("order");
        QTest::newRow("shuffle") << QString("shuffle_cycle");
        QTest::newRow("sequential") << QString("sequential");
    }

    void creatingFullDayChannelPreservesOffset()
    {
        MainWindow window;
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto *name = dialog->findChild<QLineEdit *>("channelName");
            auto *fullDay = dialog->findChild<QCheckBox *>("channelFullDay");
            QVERIFY(name && fullDay);
            name->setText(QStringLiteral("Полные_сутки"));
            fullDay->setChecked(true);
            dialog->accept();
        });
        window.slot_addChannel();
        QCOMPARE(window.mChannelManagers[0]->channelCount(), 1);
        const auto &channel = window.mChannelManagers[0]->channel(0);
        QCOMPARE(channel.startTime(), QTime(0, 0));
        QCOMPARE(channel.endTime(), QTime(0, 0));
        QCOMPARE(channel.untilDayOffset(), 1);
        QString error;
        const auto transport = window.playbackSchedule(MainWindow::PAGE_MUSIC, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(transport["channels"].toArray().first().toObject()["untilDayOffset"].toInt(), 1);
    }

    void selectedAudioChannelAndAtomicSchedule()
    {
        QFETCH(QString, order);
        Backend backend;
        const QDateTime now(QDate(2026, 10, 4), QTime(10, 24));
        MediaBox::PlayerEngine engine(&backend, nullptr, [now] { return now; }, ":memory:");
        QList<QJsonObject> requests;
        MediaBox::ControlServer server([&](const QJsonObject &request) {
            requests.append(request);
            return engine.execute(request);
        }, token().toLatin1());
        QString error;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        MainWindow window;
        seed(window, order);
        QVERIFY(!QTest::currentTestFailed());
        injectAudio(window, server.port());
        qApp->setProperty("restylePreviewStation", QVariant());
        QTRY_VERIFY(window.mMediaController->isReady());
        window.updatePlaybackActions();
        window.show();

        // Filtering the library or selecting one row must not shorten the channel queue.
        window.mPages[0].search->setText("Первый");
        QCOMPARE(window.mPages[0].files->model()->rowCount(), 1);
        window.mPages[0].files->selectionModel()->select(window.mPages[0].files->model()->index(0, 0),
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        auto *play = window.findChild<QPushButton *>("playChannelButton");
        QVERIFY(play && play->isEnabled());
        play->click();
        QTRY_COMPARE(window.mMediaController->status().channelName, QStringLiteral("Ручной_канал"));
        QCOMPARE(window.mMediaController->status().queue, filesFor("music", "Ручной_канал"));
        QCOMPARE(window.mMediaController->status().volumePercent, 63);
        QCOMPARE(window.mMediaController->status().repeat, QStringLiteral("all"));
        QCOMPARE(window.mMediaController->status().playbackMode, QStringLiteral("manual"));
        QCOMPARE(window.mMediaController->status().state, QStringLiteral("playing"));
        QCOMPARE(backend.volume, 63);
        backend.finish();
        backend.finish();
        backend.finish();
        QCOMPARE(engine.status().value("order").toString(), order);
        QVERIFY(engine.status().value("currentIndex").toInt() >= 0);
        QVERIFY(engine.status().value("currentIndex").toInt() < window.mMediaController->status().queue.size());
        QCOMPARE(engine.status().value("state").toString(), QStringLiteral("playing"));
        QVERIFY(capture(window, "music-channel"));

        const int beforeSchedule = requests.size();
        auto *scheduled = window.findChild<QPushButton *>("audioScheduleButton");
        QVERIFY(scheduled && scheduled->isEnabled());
        QVERIFY2(window.publishMusicSchedule(true, &error), qPrintable(error));
        QTRY_COMPARE(window.mMediaController->status().playbackMode, QStringLiteral("schedule"));
        QCOMPARE(requests.size(), beforeSchedule + 1);
        const QJsonObject request = requests.last();
        QCOMPARE(request.value("command").toString(), QStringLiteral("setPublication"));
        const auto snapshot = QJsonDocument::fromJson(QByteArray::fromBase64(request.value("snapshotBase64").toString().toLatin1())).object();
        QCOMPARE(snapshot.value("format").toString(), QStringLiteral("mediabox.schedule"));
        QCOMPARE(request.value("active").toObject().value("publicationId"), snapshot.value("publicationId"));
        const auto channels = snapshot.value("playlists").toArray();
        QCOMPARE(channels.size(), 2);
        QCOMPARE(channels.at(0).toObject().value("name").toString(), QStringLiteral("Дневной_канал"));
        QCOMPARE(channels.at(0).toObject().value("order").toString(), order);
        QCOMPARE(channels.at(1).toObject().value("order").toString(), order);
        QCOMPARE(channels.at(1).toObject().value("entries").toArray().size(), 3);
        const auto adverts = snapshot.value("eventRules").toArray();
        QCOMPARE(adverts.size(), 2);
        QCOMPARE(adverts.at(0).toObject().value("times").toArray(), QJsonArray({"10:00:00", "10:30:00"}));
        QCOMPARE(adverts.at(1).toObject().value("times").toArray(), QJsonArray({"10:02:00", "10:22:00", "10:42:00"}));
        QCOMPARE(window.mMediaController->status().publicationId, snapshot.value("publicationId").toString());
        QCOMPARE(window.mMediaController->status().channelName, QStringLiteral("Дневной_канал"));
        QVERIFY(filesFor("music", "Дневной_канал").contains(window.mMediaController->status().currentTrack));
        QCOMPARE(window.mMediaController->status().volumePercent, 21);
        play->click();
        QTRY_COMPARE(window.mMediaController->status().playbackMode, QStringLiteral("manual"));
        QCOMPARE(window.mMediaController->status().channelName, QStringLiteral("Ручной_канал"));
        QCOMPARE(window.mMediaController->status().queue.size(), 3);
    }

    void selectedVideoChannelTargetsWindow_data()
    {
        selectedAudioChannelAndAtomicSchedule_data();
    }

    void selectedVideoChannelTargetsWindow()
    {
        QFETCH(QString, order);
        Backend backend;
        MediaBox::PlayerEngine engine(&backend);
        QList<QJsonObject> requests;
        MediaBox::ControlServer server([&](const QJsonObject &request) {
            requests.append(request);
            auto arguments = request;
            arguments.remove("windowId");
            auto result = engine.execute(arguments);
            result.insert("status", videoStatus(result.value("status").toObject()));
            return result;
        }, token().toLatin1());
        QString error;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        MainWindow window;
        seed(window, order);
        QVERIFY(!QTest::currentTestFailed());
        window.mVideoController = new VideoController(&window);
        connect(window.mVideoController, &MediaBoxVPlayerClient::videoStatusChanged,
                &window, &MainWindow::updatePlaybackActions);
        connect(window.mVideoController, &MediaBoxPlayerClient::connectionStateChanged,
                &window, &MainWindow::updatePlaybackActions);
        window.mVideoController->setTiming({10000, 500, 1000, 1000, 1000});
        window.mVideoController->connectToPlayer({"127.0.0.1", server.port(), token()});
        qApp->setProperty("restylePreviewStation", QVariant());
        QTRY_VERIFY(window.mVideoController->isReady());
        window.changePage(MainWindow::PAGE_VIDEO);
        window.updatePlaybackActions();
        window.show();
        auto *play = window.findChild<QPushButton *>("playVideoChannelButton");
        QVERIFY(play && play->isEnabled());
        play->click();
        QTRY_COMPARE(requests.size(), 2);
        const auto request = requests.last();
        QCOMPARE(request.value("command").toString(), QStringLiteral("playChannel"));
        QCOMPARE(request.value("windowId").toString(), QStringLiteral("screen-a"));
        QCOMPARE(request.value("name").toString(), QStringLiteral("Город"));
        QCOMPARE(request.value("paths").toArray(), QJsonArray::fromStringList(filesFor("video", "Город")));
        QCOMPARE(request.value("volume").toInt(), 78);
        QCOMPARE(request.value("order").toString(), order);
        QCOMPARE(engine.status().value("order").toString(), order);
        QString scheduleError;
        const auto schedule = window.playbackSchedule(MainWindow::PAGE_VIDEO, &scheduleError);
        QVERIFY(scheduleError.isEmpty());
        QCOMPARE(schedule.value("channels").toArray().at(1).toObject().value("order").toString(), order);
        QTRY_COMPARE(window.mVideoController->videoStatus().windows.at(0).playback.repeat, QStringLiteral("all"));
        QCOMPARE(window.mVideoController->videoStatus().windows.at(0).playback.queue.size(), 3);
        QTRY_COMPARE(playingNames(window.mPages[1].source),
                     QStringList{QFileInfo(window.mVideoController->videoStatus().windows.at(0).playback.currentTrack).fileName()});
        if (auto *dialog = window.findChild<QDialog *>("videoControlDialog")) dialog->hide();
        QVERIFY(capture(window, "video-channel"));
    }

    void playingTrackFollowsAudioAndPreservesSelection()
    {
        Backend backend;
        MediaBox::PlayerEngine engine(&backend);
        MediaBox::ControlServer server(&engine, token().toLatin1());
        QString error;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        MainWindow window;
        seed(window);
        QVERIFY(!QTest::currentTestFailed());
        injectAudio(window, server.port());
        QTRY_VERIFY(window.mMediaController->isReady());
        window.show();
        auto &page = window.mPages[MainWindow::PAGE_MUSIC];
        const QStringList paths = filesFor("music", "Ручной_канал");
        auto *selection = page.files->selectionModel();
        selection->select(page.proxy->index(2, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        const QString selected = selection->selectedRows().first().data(MediaModel::FileNameRole).toString();
        QVERIFY(!window.mMediaController->load(paths, 0, true).isEmpty());
        QTRY_COMPARE(playingNames(page.source), QStringList{QFileInfo(paths[0]).fileName()});
        QCOMPARE(selection->selectedRows().first().data(MediaModel::FileNameRole).toString(), selected);
        QVERIFY(page.source->index(0, 0).data(Qt::AccessibleDescriptionRole).toString().startsWith("Сейчас играет"));
        QVERIFY(capture(window, "playing-track"));
        Restyle::apply("tide-relief", "dark");
        QVERIFY(capture(window, "playing-track-dark"));
        Restyle::apply("tide-relief", "denim");

        page.proxy->sort(0, Qt::DescendingOrder);
        QCOMPARE(playingNames(page.proxy), playingNames(page.source));
        page.search->setText(QFileInfo(paths[1]).completeBaseName());
        QCOMPARE(page.proxy->rowCount(), 1);
        QVERIFY(playingNames(page.proxy).isEmpty());
        page.search->clear();
        QCOMPARE(playingNames(page.proxy), playingNames(page.source));

        // All channels contain the same basenames; only the full path identifies playback.
        page.channels->selectRow(0);
        window.selectChannel(0, 0);
        QVERIFY(playingNames(page.source).isEmpty());
        page.channels->selectRow(1);
        window.selectChannel(0, 1);
        QCOMPARE(playingNames(page.source), QStringList{QFileInfo(paths[0]).fileName()});

        window.mMediaController->next();
        QTRY_COMPARE(playingNames(page.source), QStringList{QFileInfo(paths[1]).fileName()});
        window.mMediaController->pause();
        QTRY_VERIFY(playingNames(page.source).isEmpty());
        window.mMediaController->play();
        QTRY_COMPARE(playingNames(page.source), QStringList{QFileInfo(paths[1]).fileName()});
        window.mMediaController->stop();
        QTRY_VERIFY(playingNames(page.source).isEmpty());

        const QString advert = fixtureRoot + "/media/ads/Кофе.mp3";
        window.mMediaController->load({advert}, 0, true);
        QTRY_COMPARE(playingNames(window.mPages[2].source), QStringList{"Кофе.mp3"});
        QVERIFY(playingNames(page.source).isEmpty());
        window.mMediaController->disconnectFromPlayer();
        QVERIFY(playingNames(window.mPages[2].source).isEmpty());
    }

    void allVideoWindowsContributePlayingTracks()
    {
        Backend backend;
        MediaBox::PlayerEngine engine(&backend);
        QJsonObject status = videoStatus(engine.status());
        MediaBox::ControlServer server([&](const QJsonObject &request) {
            auto reply = engine.execute(request);
            reply.insert("status", status);
            return reply;
        }, token().toLatin1());
        QString error;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        MainWindow window;
        seed(window);
        QVERIFY(!QTest::currentTestFailed());
        window.mVideoController = new VideoController(&window);
        connect(window.mVideoController, &MediaBoxVPlayerClient::videoStatusChanged,
                &window, &MainWindow::updatePlaybackActions);
        connect(window.mVideoController, &MediaBoxPlayerClient::connectionStateChanged,
                &window, &MainWindow::updatePlaybackActions);
        const QStringList paths = filesFor("video", "Город");
        const QString advert = fixtureRoot + "/media/ads/Кофе.mp3";
        QJsonArray windows;
        for (int i = 0; i < 3; ++i) {
            const QString path = i < 2 ? paths[i] : advert;
            auto playback = engine.status();
            playback.insert("state", "playing");
            playback.insert("playbackRequested", true);
            playback.insert("queue", QJsonArray{path});
            playback.insert("currentIndex", 0);
            playback.insert("currentTrack", path);
            auto video = status.value("windows").toArray().first().toObject();
            video.insert("id", QString::number(i));
            video.insert("playback", playback);
            windows.append(video);
        }
        status.insert("windows", windows);
        window.mVideoController->connectToPlayer({"127.0.0.1", server.port(), token()});
        QTRY_VERIFY(window.mVideoController->isReady());
        const QStringList names{QFileInfo(paths[0]).fileName(), QFileInfo(paths[1]).fileName()};
        QTRY_COMPARE(playingNames(window.mPages[1].source), names);
        QCOMPARE(playingNames(window.mPages[2].source), QStringList{"Кофе.mp3"});
        windows.removeAt(0);
        status.insert("windows", windows);
        window.mVideoController->requestStatus();
        QTRY_COMPARE(playingNames(window.mPages[1].source), QStringList{names[1]});
        window.mVideoController->disconnectFromPlayer();
        QVERIFY(playingNames(window.mPages[1].source).isEmpty());
        QVERIFY(playingNames(window.mPages[2].source).isEmpty());
    }

    void folderActionsUseSelectedChannel_data()
    {
        QTest::addColumn<int>("page");
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("channel");
        QTest::newRow("music") << int(MainWindow::PAGE_MUSIC) << QString("music") << QString("Ручной_канал");
        QTest::newRow("video") << int(MainWindow::PAGE_VIDEO) << QString("video") << QString("Город");
    }

    void folderActionsUseSelectedChannel()
    {
        QFETCH(int, page);
        QFETCH(QString, kind);
        QFETCH(QString, channel);
        MainWindow window;
        seed(window);
        QVERIFY(!QTest::currentTestFailed());
        FileUrlReceiver receiver;
        QDesktopServices::setUrlHandler("file", &receiver, "receive");
        qApp->setProperty("restylePreviewStation", QVariant());
        window.changePage(page);
        window.updatePlaybackActions();
        auto *open = window.mPages[page].openFolder;
        QVERIFY(open && open->isEnabled());
        open->click();
        QCOMPARE(receiver.urls.size(), 1);
        QCOMPARE(receiver.urls.first(), QUrl::fromLocalFile(fixtureRoot + "/media/" + kind + '/' + channel));
        window.mPages[page].channels->selectRow(0);
        window.selectChannel(page, 0);
        open->click();
        QCOMPARE(receiver.urls.size(), 2);
        const QString first = page == MainWindow::PAGE_MUSIC ? "Дневной_канал" : "Природа";
        QCOMPARE(receiver.urls.last(), QUrl::fromLocalFile(fixtureRoot + "/media/" + kind + '/' + first));
    }

    void acceptedCloseSavesGeometryAndPreviewDoesNot()
    {
        MainWindow window;
        qApp->setProperty("restylePreviewStation", QVariant());
        window.show();
        window.resize(930, 670);
        window.move(20, 30);
        settle();
        const QByteArray saved = window.saveGeometry();
        QVERIFY(window.close());
        QCOMPARE(Settings().mainWindowGeometry(), saved);

        qApp->setProperty("restylePreviewStation", fixtureRoot);
        MainWindow preview;
        preview.resize(810, 610);
        preview.show();
        QVERIFY(preview.close());
        QCOMPARE(Settings().mainWindowGeometry(), saved);
    }

    void importDefersCloseWithoutSavingGeometry()
    {
        MainWindow window;
        qApp->setProperty("restylePreviewStation", QVariant());
        window.show();
        const QByteArray previous = window.saveGeometry();
        QVERIFY(Settings().setMainWindowGeometry(previous));
        window.resize(850, 650);
        window.mMediaImport = new MediaImportService(&window);
        QVERIFY(!window.close());
        QVERIFY(window.isVisible());
        QVERIFY(window.mCloseAfterImport);
        QCOMPARE(Settings().mainWindowGeometry(), previous);
        delete window.mMediaImport;
        window.mMediaImport = nullptr;
    }

    void geometryRestoresNormalAndMaximizedWindow()
    {
        qApp->setProperty("restylePreviewStation", QVariant());
        QByteArray saved;
        QSize normalSize;
        {
            MainWindow window;
            disconnectControllers(window); // Prevent deferred default player startup.
            window.show();
            window.resize(QSize(920, 660).boundedTo(window.screen()->availableGeometry().size() - QSize(40, 80)));
            settle();
            normalSize = window.size();
            QVERIFY(window.close());
            saved = Settings().mainWindowGeometry();
        }
        {
            MainWindow reopened;
            disconnectControllers(reopened);
            QCOMPARE(reopened.size(), normalSize);
            QVERIFY(!reopened.isMaximized());
            reopened.showMaximized();
            settle();
            QVERIFY(reopened.close());
        }
        {
            MainWindow maximized;
            disconnectControllers(maximized);
            QVERIFY(maximized.isMaximized());
        }
        qApp->setProperty("restylePreviewStation", fixtureRoot);
        MainWindow preview;
        QVERIFY(!preview.isMaximized());
        QVERIFY(!saved.isEmpty());
    }
};

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    const QString owner = "PlaybackUi_" + QUuid::createUuid().toString(QUuid::Id128);
    QCoreApplication::setOrganizationName(owner);
    QCoreApplication::setApplicationName("MediaBoxManagerPlaybackUiTest");
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir directory;
    if (!directory.isValid()) return 2;
    fixtureRoot = directory.path();
    application.setProperty("restylePreviewStation", fixtureRoot);
    application.setProperty("restylePreviewSettings", fixtureRoot + "/manager.conf");
    put(fixtureRoot + "/mediabox.conf", "[mediastation]\nmediabox_id=-1\nmediabox_name=Playback test\nmedia=media\n");

#ifdef Q_OS_WIN
    const QString fontPath = QStringLiteral("C:/Windows/Fonts/segoeui.ttf");
#else
    const QString fontPath = QStringLiteral("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
#endif
    const int fontId = QFontDatabase::addApplicationFont(fontPath);
    if (fontId < 0 || QFontDatabase::applicationFontFamilies(fontId).isEmpty()) return 3;
    const QString family = QFontDatabase::applicationFontFamilies(fontId).first();
    const QRawFont raw = QRawFont::fromFont(QFont(family));
    if (!raw.isValid()) return 4;
    for (QChar character : QStringLiteral("АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя—0123456789"))
        if (!raw.supportsCharacter(character)) return 5;
    application.setFont(QFont(family));
    Restyle::install(application);
    if (!Restyle::verifiedCyrillicFont()) return 6;
    PlaybackUiTests tests;
    const int result = QTest::qExec(&tests, argc, argv);
    // Geometry constructor tests use only these unique QStandardPaths test directories.
    QStringList owned{QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation),
                      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)};
    owned.removeDuplicates();
    for (const QString &path : owned)
        if (QDir::isAbsolutePath(path) && path.contains(owner)) QDir(path).removeRecursively();
    return result;
}

#include "tst_playbackui.moc"
