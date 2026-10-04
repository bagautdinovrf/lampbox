#include "mediaboxvplayerclient.h"
#include "restyletheme.h"
#include "settings.h"
#include "settingsdialog.h"
#include "videocontrolwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {
QString token() { return QString(64, QLatin1Char('b')); }

QJsonObject snapshot(const QString &state = QStringLiteral("stopped"), bool windowExists = true)
{
    const QJsonObject playback{{"state", state}, {"playbackRequested", state == "playing"},
        {"queue", QJsonArray{"/remote/первое.mp4", "/remote/второе.mp4"}},
        {"currentIndex", 0}, {"currentTrack", "/remote/первое.mp4"},
        {"positionMs", 0}, {"durationMs", 90000}, {"volumePercent", 100},
        {"muted", false}, {"repeat", "off"}, {"error", ""}};
    QJsonArray windows;
    if (windowExists)
        windows.append(QJsonObject{{"id", "lobby"}, {"name", "Холл"}, {"screen", "remote-two"},
            {"actualScreen", "remote-two"}, {"fullscreen", true}, {"playback", playback},
            {"restoreError", ""}});
    return {{"application", "MediaBoxVPlayer"}, {"windows", windows}, {"persistenceError", ""},
        {"displays", QJsonArray{QJsonObject{{"id", "remote-one"}, {"name", "Дисплей плеера 1"}, {"index", 0}},
                               QJsonObject{{"id", "remote-two"}, {"name", "Дисплей плеера 2"}, {"index", 1}}}}};
}

class Peer final : public QObject
{
public:
    struct Request { QPointer<QTcpSocket> socket; QJsonObject object; };
    Peer()
    {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (server.hasPendingConnections()) {
                auto *socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    while (socket->canReadLine())
                        requests.append({socket, QJsonDocument::fromJson(socket->readLine()).object()});
                });
            }
        });
        server.listen(QHostAddress::LocalHost, 0);
    }
    PlayerConnectionSettings settings() const { return {QStringLiteral("127.0.0.1"), server.serverPort(), token()}; }
    void answer(int index, const QJsonObject &status)
    {
        auto &request = requests[index];
        const QJsonObject response{{"protocolVersion", 1}, {"id", request.object.value("id")},
                                  {"ok", true}, {"status", status}};
        request.socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
    }
    void fail(int index, const QString &text)
    {
        auto &request = requests[index];
        const QJsonObject response{{"protocolVersion", 1}, {"id", request.object.value("id")},
            {"ok", false}, {"error", QJsonObject{{"code", "failed"}, {"message", text}}}};
        request.socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
    }
    QString command(int index) const { return requests.at(index).object.value("command").toString(); }
    QTcpServer server;
    QList<Request> requests;
};

bool capture(QWidget &widget, const QString &name)
{
    const QString directory = qEnvironmentVariable("VIDEOPLAYER_UI_CAPTURE_DIR");
    return directory.isEmpty()
        || (QDir().mkpath(directory) && widget.grab().save(QDir(directory).filePath(name + ".png")));
}
}

class VideoControlsTests final : public QObject
{
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> directory;
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        Restyle::install(*qobject_cast<QApplication *>(qApp));
    }
    void init()
    {
        directory = std::make_unique<QTemporaryDir>();
        QVERIFY(directory->isValid());
        qApp->setProperty("restylePreviewSettings", directory->filePath("settings.ini"));
        Restyle::apply("tide-relief", "denim");
    }
    void cleanup()
    {
        qApp->setProperty("restylePreviewSettings", QVariant());
        directory.reset();
    }

    void independentConnectionSettings()
    {
        Settings settings;
        QCOMPARE(settings.videoPlayerConnection().port, quint16(17656));
        QCOMPARE(settings.playerConnection().port, quint16(17655));
        const PlayerConnectionSettings audio{QStringLiteral("audio.local"), 19000, QString(64, QLatin1Char('a'))};
        QVERIFY(settings.setPlayerConnection(audio));
        SettingsDialog dialog(nullptr);
        dialog.resize(1124, 850);
        auto *host = dialog.findChild<QLineEdit *>("videoPlayerHost");
        auto *port = dialog.findChild<QSpinBox *>("videoPlayerPort");
        auto *secret = dialog.findChild<QLineEdit *>("videoPlayerToken");
        auto *save = dialog.findChild<QPushButton *>("saveVideoPlayerConnection");
        QVERIFY(host && port && secret && save);
        QCOMPARE(port->value(), 17656);
        QCOMPARE(secret->echoMode(), QLineEdit::Password);
        QSignalSpy changed(&dialog, &SettingsDialog::videoPlayerConnectionChanged);
        QSignalSpy audioChanged(&dialog, &SettingsDialog::playerConnectionChanged);
        save->click();
        QCOMPARE(changed.size(), 1);
        QVERIFY(Settings().videoPlayerConnection().token.isEmpty());
        QCOMPARE(Settings().playerConnection().token, audio.token);
        changed.clear();
        host->setText("video.local");
        save->click();
        QCOMPARE(changed.size(), 0);
        host->setText(" video.local ");
        port->setValue(17660);
        secret->setText(token());
        save->click();
        QCOMPARE(changed.size(), 1);
        QCOMPARE(audioChanged.size(), 0);
        QCOMPARE(Settings().videoPlayerConnection().host, QString("video.local"));
        QCOMPARE(Settings().videoPlayerConnection().port, quint16(17660));
        QCOMPARE(Settings().videoPlayerConnection().token, token());
        QCOMPARE(Settings().playerConnection().host, audio.host);
        QCOMPARE(Settings().playerConnection().port, audio.port);
        QSignalSpy screens(&dialog, &SettingsDialog::videoScreensRequested);
        dialog.findChild<QPushButton *>("configureVideoScreens")->click();
        QCOMPARE(screens.size(), 1);
        dialog.show();
        QVERIFY(capture(dialog, "video-settings"));
    }

    void profilesAndPlaylistsPersistSeparately()
    {
        QVERIFY(Settings().setVideoPlayerConnection({"video.invalid", 17656, {}}));
        QString firstWindowId;
        {
            VideoControlWidget widget;
            widget.resize(1100, 760);
            widget.show();
            auto *windows = widget.findChild<QListWidget *>("videoWindows");
            auto *paths = widget.findChild<QListWidget *>("videoPlaylistPaths");
            auto *playlists = widget.findChild<QComboBox *>("videoPlaylists");
            QVERIFY(windows && paths && playlists);
            widget.findChild<QPushButton *>("videoAddWindow")->click();
            QCOMPARE(windows->count(), 1);
            QVERIFY(widget.findChild<QCheckBox *>("videoFullscreen")->isChecked());
            firstWindowId = windows->item(0)->data(Qt::UserRole).toString();
            auto *name = widget.findChild<QLineEdit *>("videoWindowName");
            name->setText("Первый зал");
            QVERIFY(QMetaObject::invokeMethod(name, "editingFinished", Qt::DirectConnection));
            widget.addPlaylistPaths({"/media/первый.mp4", "C:/Video/second.mp4"});
            QCOMPARE(paths->count(), 2);
            paths->setCurrentRow(1);
            widget.findChild<QPushButton *>("videoPathUp")->click();
            QCOMPARE(paths->item(0)->text(), QString("C:/Video/second.mp4"));
            widget.findChild<QPushButton *>("videoAddPlaylist")->click();
            QCOMPARE(playlists->count(), 2);
            QCOMPARE(paths->count(), 0);
            widget.addPlaylistPaths({"/media/alternate.mp4"});
            widget.addPlaylistPaths({"relative.mp4"});
            QCOMPARE(paths->count(), 1);
            widget.findChild<QPushButton *>("videoAddWindow")->click();
            QCOMPARE(windows->count(), 2);
            QCOMPARE(playlists->count(), 1);
            QCOMPARE(paths->count(), 0);
            widget.addPlaylistPaths({"/media/second-room.mp4"});
            QVERIFY(!widget.findChild<QPushButton *>("videoApplyWindow")->isEnabled());
            QVERIFY(!widget.findChild<QPushButton *>("videoPlay")->isEnabled());
            QVERIFY(capture(widget, "video-offline"));
        }
        VideoControlWidget reopened;
        auto *windows = reopened.findChild<QListWidget *>("videoWindows");
        auto *paths = reopened.findChild<QListWidget *>("videoPlaylistPaths");
        auto *playlists = reopened.findChild<QComboBox *>("videoPlaylists");
        QCOMPARE(windows->count(), 2);
        QCOMPARE(windows->item(0)->data(Qt::UserRole).toString(), firstWindowId);
        QCOMPARE(windows->item(0)->text(), QString("Первый зал"));
        QCOMPARE(playlists->count(), 2);
        QCOMPARE(paths->count(), 1);
        QCOMPARE(paths->item(0)->text(), QString("/media/alternate.mp4"));
        playlists->setCurrentIndex(0);
        QCOMPARE(paths->count(), 2);
        QCOMPARE(paths->item(0)->text(), QString("C:/Video/second.mp4"));
        windows->setCurrentRow(1);
        QCOMPARE(playlists->count(), 1);
        QCOMPARE(paths->item(0)->text(), QString("/media/second-room.mp4"));
        paths->setCurrentRow(0);
        reopened.findChild<QPushButton *>("videoRemovePaths")->click();
        QCOMPARE(paths->count(), 0);
        windows->setCurrentRow(0);
        QCOMPARE(paths->count(), 2);
    }

    void sharedConnectionSurvivesPanelReopening()
    {
        Peer peer;
        QVERIFY(peer.server.isListening());
        QVERIFY(Settings().setVideoPlayerConnection(peer.settings()));
        MediaBoxVPlayerClient client;
        client.setTiming({60000, 2000, 5000, 60000, 60000});
        client.connectToPlayer(peer.settings());
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0, snapshot());
        QTRY_VERIFY(client.isReady());
        for (int attempt = 0; attempt < 2; ++attempt) {
            VideoControlWidget widget(nullptr, &client);
            auto *windows = widget.findChild<QListWidget *>("videoWindows");
            auto *status = widget.findChild<QLabel *>("videoConfirmedStatus");
            QCOMPARE(windows->count(), 1);
            QVERIFY(status->text().contains("остановлено"));
            QVERIFY(client.isReady());
            QVERIFY(widget.findChild<QPushButton *>("videoPlay")->isEnabled());
            QCOMPARE(peer.requests.size(), 1);
        }
        QVERIFY(client.isReady());
        QCOMPARE(peer.requests.size(), 1);
    }

    void remoteStatusAndExplicitCommands()
    {
        Peer peer;
        QVERIFY(peer.server.isListening());
        QVERIFY(Settings().setVideoPlayerConnection(peer.settings()));
        VideoControlWidget widget;
        widget.resize(1100, 760);
        widget.show();
        auto *client = widget.findChild<MediaBoxVPlayerClient *>();
        QVERIFY(client);
        client->setTiming({60000, 2000, 5000, 60000, 60000});
        auto *windows = widget.findChild<QListWidget *>("videoWindows");
        auto *displays = widget.findChild<QComboBox *>("videoDisplay");
        auto *status = widget.findChild<QLabel *>("videoConfirmedStatus");
        auto *message = widget.findChild<QLabel *>("videoCommandMessage");
        QTRY_COMPARE(peer.requests.size(), 1);
        QCOMPARE(peer.command(0), QString("status"));
        peer.answer(0, snapshot());
        QTRY_VERIFY(client->isReady());
        QCOMPARE(windows->count(), 1);
        QCOMPARE(windows->item(0)->data(Qt::UserRole).toString(), QString("lobby"));
        QCOMPARE(displays->count(), 3);
        QCOMPARE(displays->currentData().toString(), QString("remote-two"));
        QVERIFY(status->text().contains("остановлено"));
        QCOMPARE(peer.requests.size(), 1); // Opening imports profiles without mutating the player.
        QTRY_VERIFY(widget.findChild<QPushButton *>("videoPlay")->visibleRegion().contains(
                    widget.findChild<QPushButton *>("videoPlay")->rect()));
        QTRY_VERIFY(widget.findChild<QPushButton *>("videoToggleFullscreen")->visibleRegion().contains(
                    widget.findChild<QPushButton *>("videoToggleFullscreen")->rect()));
        QVERIFY(capture(widget, "video-online"));
        auto *name = widget.findChild<QLineEdit *>("videoWindowName");
        name->setText("Черновик названия");
        client->requestStatus();
        QTRY_COMPARE(peer.requests.size(), 2);
        peer.answer(1, snapshot());
        QTRY_VERIFY(!client->videoStatus().windows.isEmpty());
        QCOMPARE(name->text(), QString("Черновик названия"));
        widget.findChild<QPushButton *>("videoPlay")->click();
        QTRY_COMPARE(peer.requests.size(), 3);
        QCOMPARE(peer.command(2), QString("play"));
        QCOMPARE(peer.requests.at(2).object.value("windowId").toString(), QString("lobby"));
        QVERIFY(status->text().contains("остановлено"));
        QVERIFY(message->text().contains("ожидаем"));
        peer.answer(2, snapshot("playing"));
        QTRY_VERIFY(status->text().contains("воспроизведение"));
        QTRY_VERIFY(message->text().contains("подтверждено"));
        widget.findChild<QPushButton *>("videoLoadPlaylist")->click();
        QTRY_COMPARE(peer.requests.size(), 4);
        QCOMPARE(peer.command(3), QString("load"));
        QVERIFY(!peer.requests.at(3).object.value("autoplay").toBool(true));
        QCOMPARE(peer.requests.at(3).object.value("paths").toArray().size(), 2);
        peer.answer(3, snapshot());
        QTRY_VERIFY(widget.findChild<QPushButton *>("videoRemoveWindow")->isEnabled());
        widget.findChild<QPushButton *>("videoRemoveWindow")->click();
        QTRY_COMPARE(peer.requests.size(), 5);
        QCOMPARE(windows->count(), 1);
        peer.fail(4, "Не удалось закрыть окно");
        QTRY_VERIFY(message->text().contains("Не удалось закрыть"));
        QCOMPARE(windows->count(), 1);
        widget.findChild<QPushButton *>("videoRemoveWindow")->click();
        QTRY_COMPARE(peer.requests.size(), 6);
        peer.answer(5, snapshot("stopped", false));
        QTRY_COMPARE(windows->count(), 0);
        QVERIFY(Settings().videoWindowProfiles().isEmpty());
        widget.reloadConnection();
        QTRY_COMPARE(peer.requests.size(), 7);
        QCOMPARE(peer.command(6), QString("status"));
        peer.answer(6, snapshot());
        QTRY_VERIFY(client->isReady());
        QCOMPARE(windows->count(), 1);
        QCOMPARE(peer.requests.size(), 7);
    }

    void importedWindowPreservesDraftAndPendingDeletionTargetsId()
    {
        Peer peer;
        QVERIFY(Settings().setVideoPlayerConnection(peer.settings()));
        VideoControlWidget widget;
        auto *client = widget.findChild<MediaBoxVPlayerClient *>();
        client->setTiming({60000, 2000, 5000, 60000, 60000});
        auto *windows = widget.findChild<QListWidget *>("videoWindows");
        auto *name = widget.findChild<QLineEdit *>("videoWindowName");
        auto *message = widget.findChild<QLabel *>("videoCommandMessage");
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0, snapshot());
        QTRY_VERIFY(client->isReady());
        name->setText("Новое название холла");
        auto twoWindows = snapshot();
        auto second = twoWindows.value("windows").toArray().first().toObject();
        second["id"] = "second";
        second["name"] = "Второй зал";
        twoWindows["windows"] = QJsonArray{snapshot().value("windows").toArray().first(), second};
        client->requestStatus();
        QTRY_COMPARE(peer.requests.size(), 2);
        peer.answer(1, twoWindows);
        QTRY_COMPARE(windows->count(), 2);
        QCOMPARE(name->text(), QString("Новое название холла"));
        widget.findChild<QPushButton *>("videoApplyWindow")->click();
        QTRY_COMPARE(peer.requests.size(), 3);
        QCOMPARE(peer.command(2), QString("configureWindow"));
        QCOMPARE(peer.requests.at(2).object.value("windowId").toString(), QString("lobby"));
        QCOMPARE(peer.requests.at(2).object.value("name").toString(), QString("Новое название холла"));
        QVERIFY(peer.requests.at(2).object.value("fullscreen").toBool());
        QVERIFY(message->text().contains("ожидаем"));
        peer.answer(2, twoWindows);
        QTRY_VERIFY(widget.findChild<QPushButton *>("videoToggleFullscreen")->isEnabled());
        widget.findChild<QPushButton *>("videoToggleFullscreen")->click();
        QTRY_COMPARE(peer.requests.size(), 4);
        QCOMPARE(peer.command(3), QString("fullscreen"));
        QVERIFY(!peer.requests.at(3).object.value("value").toBool(true));
        peer.answer(3, twoWindows);
        QTRY_VERIFY(widget.findChild<QPushButton *>("videoRemoveWindow")->isEnabled());
        widget.findChild<QPushButton *>("videoRemoveWindow")->click();
        QTRY_COMPARE(peer.requests.size(), 5);
        QCOMPARE(peer.requests.at(4).object.value("windowId").toString(), QString("lobby"));
        windows->setCurrentRow(1);
        QCOMPARE(name->text(), QString("Второй зал"));
        twoWindows["windows"] = QJsonArray{second};
        peer.answer(4, twoWindows);
        QTRY_COMPARE(windows->count(), 1);
        QCOMPARE(windows->item(0)->data(Qt::UserRole).toString(), QString("second"));
        QCOMPARE(Settings().videoWindowProfiles().first().toObject().value("id").toString(), QString("second"));
    }
};

QTEST_MAIN(VideoControlsTests)
#include "tst_videocontrols.moc"
