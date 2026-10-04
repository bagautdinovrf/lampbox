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

    void seed(MainWindow &window)
    {
        for (int page = MainWindow::PAGE_MUSIC; page <= MainWindow::PAGE_VIDEO; ++page) {
            auto *manager = window.mChannelManagers[page];
            const QStringList names = page == MainWindow::PAGE_MUSIC
                ? QStringList{"Дневной_канал", "Ручной_канал"} : QStringList{"Природа", "Город"};
            for (int row = 0; row < names.size(); ++row) {
                window.mPages[page].source->setMediaManager(nullptr);
                // The second channel is inactive in October, but remains manually playable.
                const QString months = row == 0 ? "*" : "1";
                const int volume = row == 0 ? 21 : page == MainWindow::PAGE_MUSIC ? 63 : 78;
                QVERIFY2(manager->createChannel({names[row], QTime(0, 0), QTime(23, 59),
                    "*", "*", months, volume}), qPrintable(manager->lastError()));
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

    void selectedAudioChannelAndAtomicSchedule()
    {
        Backend backend;
        const QDateTime now(QDate(2026, 10, 4), QTime(10, 24));
        MediaBox::PlayerEngine engine(&backend, nullptr, [now] { return now; });
        QList<QJsonObject> requests;
        MediaBox::ControlServer server([&](const QJsonObject &request) {
            requests.append(request);
            return engine.execute(request);
        }, token().toLatin1());
        QString error;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        MainWindow window;
        seed(window);
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
        QCOMPARE(engine.status().value("currentIndex").toInt(), 0);
        QCOMPARE(engine.status().value("state").toString(), QStringLiteral("playing"));
        QVERIFY(capture(window, "music-channel"));

        const int beforeSchedule = requests.size();
        auto *scheduled = window.findChild<QPushButton *>("audioScheduleButton");
        QVERIFY(scheduled && scheduled->isEnabled());
        scheduled->click();
        QTRY_COMPARE(window.mMediaController->status().playbackMode, QStringLiteral("schedule"));
        QCOMPARE(requests.size(), beforeSchedule + 1);
        const QJsonObject request = requests.last();
        QCOMPARE(request.value("command").toString(), QStringLiteral("schedule"));
        const auto snapshot = request.value("schedule").toObject();
        const auto channels = snapshot.value("channels").toArray();
        QCOMPARE(channels.size(), 2);
        QCOMPARE(channels.at(0).toObject().value("name").toString(), QStringLiteral("Дневной_канал"));
        QCOMPARE(channels.at(1).toObject().value("paths").toArray(),
                 QJsonArray::fromStringList(filesFor("music", "Ручной_канал")));
        const auto adverts = snapshot.value("adverts").toArray();
        QCOMPARE(adverts.size(), 2);
        QCOMPARE(adverts.at(1).toObject().value("compiledMinutes").toArray(), QJsonArray({2, 22, 42}));
        QCOMPARE(adverts.at(1).toObject().value("paths").toArray(),
                 QJsonArray({fixtureRoot + "/media/ads/Кофе.mp3"}));
        QCOMPARE(window.mMediaController->status().channelName, QStringLiteral("Дневной_канал"));
        QCOMPARE(window.mMediaController->status().queue, filesFor("music", "Дневной_канал"));
        QCOMPARE(window.mMediaController->status().volumePercent, 21);
        play->click();
        QTRY_COMPARE(window.mMediaController->status().playbackMode, QStringLiteral("manual"));
        QCOMPARE(window.mMediaController->status().channelName, QStringLiteral("Ручной_канал"));
        QCOMPARE(window.mMediaController->status().queue.size(), 3);
    }

    void selectedVideoChannelTargetsWindow()
    {
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
        seed(window);
        QVERIFY(!QTest::currentTestFailed());
        window.mVideoController = new VideoController(&window);
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
        QTRY_COMPARE(window.mVideoController->videoStatus().windows.at(0).playback.repeat, QStringLiteral("all"));
        QCOMPARE(window.mVideoController->videoStatus().windows.at(0).playback.queue.size(), 3);
        if (auto *dialog = window.findChild<QDialog *>("videoControlDialog")) dialog->hide();
        QVERIFY(capture(window, "video-channel"));
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
