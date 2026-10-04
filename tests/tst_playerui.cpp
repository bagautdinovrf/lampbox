#include "mediaboxplayerclient.h"
#include "playercontrolwidget.h"
#include "restyletheme.h"
#include "settings.h"
#include "settingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <memory>

namespace {
using Client = MediaBoxPlayerClient;

QString token() { return QString(64, QLatin1Char('a')); }

QJsonObject snapshot(const QString &state = QStringLiteral("stopped"))
{
    return {{"state", state}, {"playbackRequested", state == "playing" || state == "loading"},
            {"queue", QJsonArray{"/srv/music/Первая песня.wav", "C:/Media/Вторая песня.wav"}},
            {"currentIndex", 0}, {"currentTrack", "/srv/music/Первая песня.wav"},
            {"positionMs", 12345}, {"durationMs", 180000}, {"volumePercent", 70},
            {"muted", false}, {"repeat", "off"}, {"error", ""},
            {"playbackMode", "manual"}, {"channelName", ""},
            {"scheduleAvailable", false}, {"scheduleError", ""},
            {"publicationId", ""}, {"scheduleId", ""}, {"revision", 0},
            {"supportedCapabilities", QJsonArray{"schedule.current.v1"}}};
}

// Keep replies under test control to observe the UI before and after the
// player confirms a command. Authenticated frames are never logged.
class Peer final : public QObject
{
public:
    struct Request { QPointer<QTcpSocket> socket; QJsonObject object; };
    Peer()
    {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (server.hasPendingConnections()) {
                QTcpSocket *socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    while (socket->canReadLine())
                        requests.append({socket, QJsonDocument::fromJson(socket->readLine()).object()});
                });
            }
        });
        listening = server.listen(QHostAddress::LocalHost, 0);
    }
    PlayerConnectionSettings settings() const
    {
        return {QStringLiteral("127.0.0.1"), server.serverPort(), token()};
    }
    void answer(int index, const QJsonObject &status)
    {
        const Request &request = requests.at(index);
        const QJsonObject reply{{"protocolVersion", 1}, {"id", request.object.value("id")},
                                {"ok", true}, {"status", status}};
        request.socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
        request.socket->flush();
    }
    void abort(int index) { requests.at(index).socket->abort(); }
    QString command(int index) const { return requests.at(index).object.value("command").toString(); }
    QTcpServer server;
    QList<Request> requests;
    bool listening = false;
};

void configure(Client &client)
{
    client.setTiming({60000, 2000, 5000, 60000, 60000});
}

bool capture(QWidget &widget, const QString &name)
{
    const QString directory = qEnvironmentVariable("PLAYER_UI_CAPTURE_DIR");
    return directory.isEmpty()
            || (QDir().mkpath(directory) && widget.grab().save(QDir(directory).filePath(name + ".png")));
}
}

class PlayerUiTests final : public QObject
{
    Q_OBJECT
private:
    std::unique_ptr<QTemporaryDir> mDirectory;

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        Restyle::install(*qobject_cast<QApplication *>(qApp));
    }

    void init()
    {
        mDirectory = std::make_unique<QTemporaryDir>();
        QVERIFY(mDirectory->isValid());
        qApp->setProperty("restylePreviewSettings", mDirectory->filePath("settings.ini"));
        Restyle::apply("tide-relief", "denim");
    }

    void cleanup()
    {
        qApp->setProperty("restylePreviewSettings", QVariant());
        mDirectory.reset();
    }

    void connectionSettingsRequireValidExplicitSave()
    {
        SettingsDialog dialog(nullptr);
        dialog.resize(1124, 850);
        dialog.show();
        auto *host = dialog.findChild<QLineEdit *>("playerHost");
        auto *port = dialog.findChild<QSpinBox *>("playerPort");
        auto *secret = dialog.findChild<QLineEdit *>("playerToken");
        auto *save = dialog.findChild<QPushButton *>("savePlayerConnection");
        auto *message = dialog.findChild<QLabel *>("playerConnectionMessage");
        QVERIFY(host && port && secret && save && message);
        QSignalSpy changed(&dialog, &SettingsDialog::playerConnectionChanged);
        QCOMPARE(host->text(), QString("127.0.0.1"));
        QCOMPARE(port->value(), 17655);
        QCOMPARE(secret->echoMode(), QLineEdit::Password);
        QCOMPARE(port->minimum(), 1);
        QCOMPARE(port->maximum(), 65535);

        save->click();
        QCOMPARE(changed.size(), 1);
        QVERIFY(Settings().playerConnection().token.isEmpty());
        changed.clear();
        QVERIFY(!message->text().isEmpty());
        host->setText("player.example.test");
        save->click();
        QCOMPARE(changed.size(), 0);
        QCOMPARE(Settings().playerConnection().host, QString("127.0.0.1"));
        host->setText("127.0.0.1");
        secret->setText(QString(64, QLatin1Char('A')));
        save->click();
        QCOMPARE(changed.size(), 0);
        secret->setText(token());
        host->setText("   ");
        save->click();
        QCOMPARE(changed.size(), 0);

        host->setText(" player.example.test ");
        port->setValue(24567);
        QCOMPARE(Settings().playerConnection().host, QString("127.0.0.1"));
        Restyle::apply("tide", "dark");
        QCOMPARE(secret->text(), token());
        QCOMPARE(host->text(), QString(" player.example.test "));
        save->click();
        QCOMPARE(changed.size(), 1);
        const auto saved = Settings().playerConnection();
        QCOMPARE(saved.host, QString("player.example.test"));
        QCOMPARE(saved.port, quint16(24567));
        QVERIFY(saved.token == token());
        QVERIFY(!message->text().contains(token()));
        QCOMPARE(secret->echoMode(), QLineEdit::Password);
        Restyle::apply("tide-relief", "denim");
        QVERIFY(capture(dialog, "settings"));
    }

    void settingsFailureDoesNotReconnect()
    {
        SettingsDialog dialog(nullptr);
        auto *secret = dialog.findChild<QLineEdit *>("playerToken");
        auto *save = dialog.findChild<QPushButton *>("savePlayerConnection");
        auto *message = dialog.findChild<QLabel *>("playerConnectionMessage");
        QVERIFY(secret && save && message);
        secret->setText(token());
        QSignalSpy changed(&dialog, &SettingsDialog::playerConnectionChanged);
        const QString config = mDirectory->filePath("settings.ini");
        QVERIFY(QFile::remove(config));
        QVERIFY(QDir().mkpath(config));
        save->click();
        QCOMPARE(changed.size(), 0);
        QVERIFY(message->text().contains(QStringLiteral("Не удалось сохранить")));
        QVERIFY(!message->text().contains(token()));
    }

    void playbackUsesConfirmedStateAndRetainsStaleSnapshot()
    {
        Peer peer;
        QVERIFY(peer.listening);
        Client client;
        configure(client);
        PlayerControlWidget panel(&client);
        panel.resize(780, 720);
        panel.show();
        auto *play = panel.findChild<QPushButton *>("playerPlayButton");
        auto *state = panel.findChild<QLabel *>("playerPlaybackState");
        auto *intent = panel.findChild<QLabel *>("playerPlaybackIntent");
        auto *audioError = panel.findChild<QLabel *>("playerAudioError");
        auto *freshness = panel.findChild<QLabel *>("playerStatusFreshness");
        auto *track = panel.findChild<QLineEdit *>("playerCurrentTrack");
        auto *queue = panel.findChild<QListWidget *>("playerQueueList");
        QVERIFY(play && state && intent && audioError && freshness && track && queue);
        QVERIFY(!play->isEnabled());
        client.connectToPlayer(peer.settings());
        QTRY_COMPARE(peer.requests.size(), 1);
        QCOMPARE(peer.command(0), QString("status"));
        QVERIFY(!play->isEnabled());
        peer.answer(0, snapshot());
        QTRY_VERIFY(client.isReady());
        QVERIFY(play->isEnabled());
        QCOMPARE(queue->count(), 2);
        QCOMPARE(state->text(), QStringLiteral("Остановлен"));
        play->click();
        QTRY_COMPARE(peer.requests.size(), 2);
        QCOMPARE(peer.command(1), QString("play"));
        QCOMPARE(state->text(), QStringLiteral("Остановлен"));
        peer.answer(1, snapshot("loading"));
        QTRY_COMPARE(state->text(), QStringLiteral("Загрузка"));
        QVERIFY(!intent->text().isEmpty());

        client.requestStatus();
        QTRY_COMPARE(peer.requests.size(), 3);
        auto failed = snapshot("error");
        failed["playbackRequested"] = true;
        failed["error"] = QStringLiteral("Не удалось открыть аудиоустройство");
        peer.answer(2, failed);
        QTRY_COMPARE(state->text(), QStringLiteral("Ошибка аудио"));
        QCOMPARE(audioError->text(), failed.value("error").toString());
        QVERIFY(!audioError->isHidden());
        QVERIFY(!intent->text().isEmpty());

        client.requestStatus();
        QTRY_COMPARE(peer.requests.size(), 4);
        peer.answer(3, snapshot("playing"));
        QTRY_COMPARE(state->text(), QStringLiteral("Воспроизводится"));
        QVERIFY(intent->text().isEmpty());
        QVERIFY(audioError->isHidden());
        QVERIFY(capture(panel, "playing"));
        client.disconnectFromPlayer();
        QVERIFY(freshness->text().contains(QStringLiteral("устаревший")));
        QVERIFY(state->text().startsWith(QStringLiteral("Последнее:")));
        QCOMPARE(track->text(), snapshot().value("currentTrack").toString());
        QCOMPARE(queue->count(), 2);
        const QStringList disabled{"playerPlayButton", "playerPauseButton", "playerStopButton",
                                   "playerNextButton", "playerPreviousButton", "playerLoadButton",
                                   "playerEnqueueButton", "playerClearButton", "playerRefreshButton",
                                   "playerSeekSlider", "playerVolumeSlider", "playerMuteCheckBox",
                                   "playerRepeatComboBox"};
        for (const QString &name : disabled) {
            auto *control = panel.findChild<QWidget *>(name);
            QVERIFY2(control, qPrintable(name));
            QVERIFY2(!control->isEnabled(), qPrintable(name));
        }
        QVERIFY(capture(panel, "offline"));
    }

    void soundControlsWaitForConfirmationWithoutFeedbackCommands()
    {
        Peer peer;
        QVERIFY(peer.listening);
        Client client;
        configure(client);
        PlayerControlWidget panel(&client);
        panel.resize(780, 720);
        panel.show();
        auto *mute = panel.findChild<QCheckBox *>("playerMuteCheckBox");
        auto *repeat = panel.findChild<QComboBox *>("playerRepeatComboBox");
        auto *volume = panel.findChild<QSlider *>("playerVolumeSlider");
        QVERIFY(mute && repeat && volume);
        client.connectToPlayer(peer.settings());
        QTRY_COMPARE(peer.requests.size(), 1);
        auto current = snapshot("playing");
        peer.answer(0, current);
        QTRY_VERIFY(client.isReady());
        QCOMPARE(volume->value(), 70);
        QVERIFY(!mute->isChecked());
        mute->click();
        QTRY_COMPARE(peer.requests.size(), 2);
        QCOMPARE(peer.command(1), QString("mute"));
        QCOMPARE(peer.requests.at(1).object.value("value").toBool(), true);
        QVERIFY(!mute->isChecked());
        current["muted"] = true;
        peer.answer(1, current);
        QTRY_VERIFY(mute->isChecked());

        repeat->setFocus();
        QTest::keyClick(repeat, Qt::Key_End);
        QTRY_COMPARE(peer.requests.size(), 3);
        QCOMPARE(peer.command(2), QString("repeat"));
        QCOMPARE(peer.requests.at(2).object.value("mode").toString(), QString("one"));
        QCOMPARE(repeat->currentData().toString(), QString("off"));
        current["repeat"] = "one";
        peer.answer(2, current);
        QTRY_COMPARE(repeat->currentData().toString(), QString("one"));

        volume->setFocus();
        QTest::keyClick(volume, Qt::Key_Right);
        QTRY_COMPARE(peer.requests.size(), 4);
        QCOMPARE(peer.command(3), QString("volume"));
        QCOMPARE(peer.requests.at(3).object.value("value").toInt(), 71);
        QCOMPARE(volume->value(), 70);
        current["volumePercent"] = 71;
        peer.answer(3, current);
        QTRY_COMPARE(volume->value(), 71);
        // A following explicit refresh forms a barrier: UI updates must not
        // have queued another mute/repeat/volume mutation.
        client.requestStatus();
        QTRY_COMPARE(peer.requests.size(), 5);
        QCOMPARE(peer.command(4), QString("status"));
        peer.answer(4, current);
    }

    void queueReplacementRequiresExplicitRemotePathsAndStopsPlayback()
    {
        Peer peer;
        QVERIFY(peer.listening);
        Client client;
        configure(client);
        PlayerControlWidget panel(&client);
        auto *load = panel.findChild<QPushButton *>("playerLoadButton");
        auto *queue = panel.findChild<QListWidget *>("playerQueueList");
        QVERIFY(load && queue);
        client.connectToPlayer(peer.settings());
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0, snapshot("playing"));
        QTRY_VERIFY(client.isReady());
        QCOMPARE(peer.requests.size(), 1); // Opening the panel never loads.
        bool found = false;
        bool rejectedRelative = false;
        bool explicitStop = false;
        QTimer::singleShot(0, &panel, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            auto *paths = dialog->findChild<QPlainTextEdit *>("playerRemotePathsEdit");
            auto *submit = dialog->findChild<QPushButton *>("playerSubmitPathsButton");
            auto *error = dialog->findChild<QLabel *>("playerPathsError");
            found = dialog->objectName() == "playerPathsDialog" && paths && submit && error;
            if (!found) { dialog->reject(); return; }
            explicitStop = submit->text().contains(QStringLiteral("остановить"));
            paths->setPlainText("relative/song.wav");
            submit->click();
            rejectedRelative = dialog->isVisible() && !error->isHidden() && peer.requests.size() == 1;
            paths->setPlainText("/remote/Новая песня.wav\nC:/Player/Трек.wav");
            submit->click();
            if (dialog->isVisible()) dialog->reject();
        });
        load->click();
        QVERIFY(found);
        QVERIFY(explicitStop);
        QVERIFY(rejectedRelative);
        QTRY_COMPARE(peer.requests.size(), 2);
        const QJsonObject request = peer.requests.at(1).object;
        QCOMPARE(peer.command(1), QString("load"));
        QCOMPARE(request.value("paths").toArray(), (QJsonArray{"/remote/Новая песня.wav", "C:/Player/Трек.wav"}));
        QVERIFY(request.value("autoplay").isBool());
        QVERIFY(!request.value("autoplay").toBool());
        QCOMPARE(request.value("startIndex").toInt(-1), 0);
        QCOMPARE(queue->item(0)->text(), QStringLiteral("1. /srv/music/Первая песня.wav"));
        auto replaced = snapshot();
        replaced["queue"] = request.value("paths");
        replaced["currentTrack"] = "/remote/Новая песня.wav";
        peer.answer(1, replaced);
        QTRY_COMPARE(queue->item(0)->text(), QStringLiteral("1. /remote/Новая песня.wav"));

        QTimer::singleShot(0, &panel, [] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
        });
        load->click();
        client.requestStatus();
        QTRY_COMPARE(peer.requests.size(), 3);
        QCOMPARE(peer.command(2), QString("status"));
        peer.answer(2, replaced);
    }

    void unknownOutcomeSurvivesReconnectAndOtherCommands()
    {
        Peer peer;
        QVERIFY(peer.listening);
        Client client;
        configure(client);
        PlayerControlWidget panel(&client);
        auto *next = panel.findChild<QPushButton *>("playerNextButton");
        auto *pause = panel.findChild<QPushButton *>("playerPauseButton");
        auto *notice = panel.findChild<QLabel *>("playerCommandResult");
        auto *dismiss = panel.findChild<QPushButton *>("playerDismissNoticeButton");
        QVERIFY(next && pause && notice && dismiss);
        client.connectToPlayer(peer.settings());
        QTRY_COMPARE(peer.requests.size(), 1);
        peer.answer(0, snapshot("playing"));
        QTRY_VERIFY(client.isReady());
        next->click();
        QTRY_COMPARE(peer.requests.size(), 2);
        QCOMPARE(peer.command(1), QString("next"));
        peer.abort(1);
        QTRY_VERIFY(notice->text().contains(QStringLiteral("Результат неизвестен")));
        const QString unknown = notice->text();
        QVERIFY(!next->isEnabled());
        client.connectToPlayer(peer.settings());
        QTRY_COMPARE(peer.requests.size(), 3);
        QCOMPARE(peer.command(2), QString("status"));
        peer.answer(2, snapshot("playing"));
        QTRY_VERIFY(client.isReady());
        QCOMPARE(notice->text(), unknown);
        pause->click();
        QTRY_COMPARE(peer.requests.size(), 4);
        QCOMPARE(peer.command(3), QString("pause"));
        peer.answer(3, snapshot("paused"));
        QTRY_VERIFY(notice->text().contains(QStringLiteral("команда принята")));
        QVERIFY(notice->text().contains(unknown));
        dismiss->click();
        QVERIFY(notice->text().isEmpty());
    }
};

QTEST_MAIN(PlayerUiTests)
#include "tst_playerui.moc"
