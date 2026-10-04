#include "controlserver.h"
#include "mediacontroller.h"
#include "settings.h"
#include "storagepaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <memory>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <csignal>
#endif

namespace {
using State = MediaBoxPlayerClient::ConnectionState;

QString testToken() { return QString(64, QLatin1Char('a')); }

quint16 freePort()
{
    QTcpServer reservation;
    return reservation.listen(QHostAddress::LocalHost, 0) ? reservation.serverPort() : 0;
}

void configure(MediaController &controller)
{
    controller.setTiming({10000, 300, 1000, 60, 120});
}

QJsonObject stoppedReply(const QJsonObject &)
{
    return {{"ok", true}, {"status", QJsonObject{
        {"state", "stopped"}, {"playbackRequested", false}, {"queue", QJsonArray{}},
        {"currentIndex", -1}, {"currentTrack", ""}, {"positionMs", 0}, {"durationMs", 0},
        {"volumePercent", 100}, {"muted", false}, {"repeat", "off"}, {"error", ""},
            {"playbackMode", "manual"}, {"channelName", ""},
            {"scheduleAvailable", false}, {"scheduleError", ""},
            {"publicationId", ""}, {"scheduleId", ""}, {"revision", 0},
            {"supportedCapabilities", QJsonArray{"schedule.current.v1"}}}}};
}

QString managerDirectory()
{
    return MediaBox::StoragePaths::configurationDirectory(MediaBox::StoragePaths::Application::Manager);
}

QString playerDirectory()
{
    return MediaBox::StoragePaths::configurationDirectory(MediaBox::StoragePaths::Application::Player);
}

QString managedDirectory(quint16 port)
{
    return QDir(managerDirectory()).filePath(QStringLiteral("managed-player/127.0.0.1-%1").arg(port));
}

class OwnedProcess final {
public:
    explicit OwnedProcess(qint64 processId) : id(processId)
    {
#ifdef Q_OS_WIN
        // Hold the original process handle from its start signal. A short-lived
        // failed launch may exit and its numeric PID must never target a later
        // unrelated process during cleanup.
        process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, DWORD(processId));
#endif
    }
    ~OwnedProcess() { stop(); }
    void stop()
    {
        // Production intentionally detaches. Only tests stop their own children,
        // so they leave no audio processes behind on success or failure.
#ifdef Q_OS_WIN
        if (process) {
            TerminateProcess(process, 0);
            WaitForSingleObject(process, 5000);
            CloseHandle(process);
            process = nullptr;
        }
#else
        if (id)
            ::kill(pid_t(id), SIGTERM);
#endif
        id = 0;
    }
    qint64 id;
private:
#ifdef Q_OS_WIN
    HANDLE process = nullptr;
#endif
};
}

class PlayerAutostartTests final : public QObject
{
    Q_OBJECT
private:
    QList<std::shared_ptr<OwnedProcess>> mProcesses;
    std::unique_ptr<QTemporaryDir> mSettingsDirectory;
    QString mOwnedRoot;

    void track(MediaController &controller)
    {
        connect(&controller, &MediaController::localPlayerStarted, this,
                [this](qint64 processId) { mProcesses.append(std::make_shared<OwnedProcess>(processId)); });
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        // StoragePaths test roots include the application name. Every case owns
        // a fresh root, including any simulated standalone/service credentials.
        QCoreApplication::setApplicationName(QStringLiteral("MediaBoxAutostartTests-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces));
        mOwnedRoot = MediaBox::StoragePaths::commonConfigurationDirectory();
        mSettingsDirectory = std::make_unique<QTemporaryDir>();
        QVERIFY(mSettingsDirectory->isValid());
        qApp->setProperty("restylePreviewSettings", mSettingsDirectory->filePath("settings.ini"));
    }

    void cleanup()
    {
        mProcesses.clear();
        qApp->setProperty("restylePreviewSettings", QVariant());
        mSettingsDirectory.reset();
        QVERIFY(mOwnedRoot.contains(QCoreApplication::applicationName()));
        if (QDir(mOwnedRoot).exists())
            QVERIFY(QDir(mOwnedRoot).removeRecursively());
    }

    void recognizesOnlyExplicitLoopbackHosts()
    {
        for (const QString &host : {QString("127.0.0.1"), QString("127.4.2.1"), QString("::1"),
                                   QString(" localhost "), QString("LOCALHOST")})
            QVERIFY2(MediaController::isLocalHost(host), qPrintable(host));
        for (const QString &host : {QString("192.168.1.4"), QString("0.0.0.0"), QString("::"),
                                   QString("player.example.test"), QString(""), QString("localhost.example")})
            QVERIFY2(!MediaController::isLocalHost(host), qPrintable(host));
    }

    void defaultLocalConnectionStartsPlayerAndSurvivesManagerRestart()
    {
        const quint16 port = freePort();
        QVERIFY(port);
        QVERIFY(Settings().setPlayerConnection({"localhost", port, {}}));
        QByteArray token;
        {
            MediaController controller;
            configure(controller);
            track(controller);
            QSignalSpy launched(&controller, &MediaController::localPlayerStarted);
            QTRY_VERIFY_WITH_TIMEOUT(controller.isReady(), 15000);
            QCOMPARE(launched.size(), 1);
            QCOMPARE(controller.status().state, QString("stopped"));
            QVERIFY(!controller.status().playbackRequested);
            QFile file(QDir(managedDirectory(port)).filePath("control.token"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            token = file.readAll().trimmed();
            QCOMPARE(token.size(), 64);
            QVERIFY(Settings().playerConnection().token.isEmpty());
            QCOMPARE(Settings().playerConnection().host, QString("localhost"));
            controller.setVolume(37);
            QTRY_COMPARE(controller.status().volumePercent, 37);
        }
        // Destruction only closes the transport; the detached engine retains
        // its state and the next Manager authenticates with the same token.
        MediaController nextManager;
        configure(nextManager);
        track(nextManager);
        QSignalSpy launchedAgain(&nextManager, &MediaController::localPlayerStarted);
        QTRY_VERIFY_WITH_TIMEOUT(nextManager.isReady(), 5000);
        QCOMPARE(launchedAgain.size(), 0);
        QCOMPARE(nextManager.status().volumePercent, 37);
        QVERIFY(!nextManager.status().playbackRequested);
        QFile file(QDir(managedDirectory(port)).filePath("control.token"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().trimmed() == token);
    }

    void disconnectCancelsScheduledAutomaticLaunch()
    {
        const quint16 port = freePort();
        QVERIFY(port);
        MediaController controller;
        configure(controller);
        track(controller);
        QSignalSpy launched(&controller, &MediaController::localPlayerStarted);
        QSignalSpy failed(&controller, &MediaBoxPlayerClient::connectionAttemptFailed);
        connect(&controller, &MediaBoxPlayerClient::connectionAttemptFailed,
                &controller, &MediaController::disconnectFromPlayer);
        controller.connectToPlayer({"127.0.0.1", port, testToken()});
        QTRY_VERIFY(!failed.isEmpty());
        QCOMPARE(controller.connectionState(), State::Disconnected);
        QTest::qWait(250);
        QCOMPARE(launched.size(), 0);
    }

    void establishedAuthenticationFailureNeverLaunchesPlayer()
    {
        MediaBox::ControlServer peer(stoppedReply, QByteArray(64, 'b'));
        QString error;
        QVERIFY(peer.listen(QHostAddress::LocalHost, 0, &error));
        MediaController controller;
        configure(controller);
        track(controller);
        QSignalSpy launched(&controller, &MediaController::localPlayerStarted);
        QSignalSpy failed(&controller, &MediaBoxPlayerClient::connectionAttemptFailed);
        controller.connectToPlayer({"127.0.0.1", peer.port(), testToken()});
        QTRY_COMPARE(controller.connectionState(), State::AuthenticationFailed);
        QCOMPARE(failed.size(), 0);
        QCOMPARE(launched.size(), 0);
    }

    void remoteFailureNeverLaunchesLocalPlayer()
    {
        MediaController controller;
        configure(controller);
        track(controller);
        QSignalSpy launched(&controller, &MediaController::localPlayerStarted);
        QSignalSpy failed(&controller, &MediaBoxPlayerClient::connectionAttemptFailed);
        controller.connectToPlayer({"player.invalid", 17655, testToken()});
        QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty(), 3000);
        QTest::qWait(150);
        QCOMPARE(launched.size(), 0);
    }

    void establishedProtocolFailureNeverLaunchesPlayer()
    {
        QTcpServer peer;
        QVERIFY(peer.listen(QHostAddress::LocalHost, 0));
        connect(&peer, &QTcpServer::newConnection, &peer, [&peer] {
            auto *socket = peer.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                if (!socket->canReadLine())
                    return;
                const auto request = QJsonDocument::fromJson(socket->readLine()).object();
                const QJsonObject reply{{"protocolVersion", 2}, {"id", request.value("id")}, {"ok", false}};
                socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
            });
        });
        MediaController controller;
        configure(controller);
        track(controller);
        QSignalSpy launched(&controller, &MediaController::localPlayerStarted);
        QSignalSpy failed(&controller, &MediaBoxPlayerClient::connectionAttemptFailed);
        controller.connectToPlayer({"127.0.0.1", peer.serverPort(), testToken()});
        QTRY_COMPARE(controller.connectionState(), State::ProtocolMismatch);
        QCOMPARE(failed.size(), 0);
        QCOMPARE(launched.size(), 0);
    }

    void failedLaunchDoesNotRepeatUntilExplicitReconnect()
    {
        const quint16 port = freePort();
        QVERIFY(port);
        QVERIFY(QDir().mkpath(managedDirectory(port)));
        QLockFile lock(QDir(managedDirectory(port)).filePath("player.lock"));
        QVERIFY(lock.tryLock());
        MediaController controller;
        configure(controller);
        track(controller);
        QSignalSpy launched(&controller, &MediaController::localPlayerStarted);
        QSignalSpy failed(&controller, &MediaBoxPlayerClient::connectionAttemptFailed);
        controller.connectToPlayer({"127.0.0.1", port, testToken()});
        QTRY_COMPARE(launched.size(), 1);
        QTRY_VERIFY(failed.size() >= 4);
        QCOMPARE(launched.size(), 1);
        controller.disconnectFromPlayer();
        lock.unlock();
        controller.connectToPlayer({"127.0.0.1", port, testToken()});
        QTRY_VERIFY_WITH_TIMEOUT(controller.isReady(), 15000);
        QCOMPARE(launched.size(), 2);
    }

    void previouslyReadyPlayerCanRestartAfterCrash()
    {
        const quint16 port = freePort();
        QVERIFY(port);
        MediaController controller;
        configure(controller);
        track(controller);
        QSignalSpy launched(&controller, &MediaController::localPlayerStarted);
        controller.connectToPlayer({"127.0.0.1", port, testToken()});
        QTRY_VERIFY_WITH_TIMEOUT(controller.isReady(), 15000);
        QCOMPARE(launched.size(), 1);
        const qint64 firstPid = launched.at(0).at(0).toLongLong();
        for (const auto &process : std::as_const(mProcesses)) {
            if (process->id == firstPid)
                process->stop();
        }
        QTRY_COMPARE_WITH_TIMEOUT(launched.size(), 2, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(controller.isReady(), 15000);
        QVERIFY(!controller.status().playbackRequested);
    }

    void defaultPortUsesExistingStandaloneToken()
    {
        QVERIFY(QDir().mkpath(playerDirectory()));
        QByteArray token;
        QString error;
        QVERIFY(MediaBox::loadControlToken(playerDirectory(), &token, &error));
        MediaBox::ControlServer peer(stoppedReply, token);
        if (!peer.listen(QHostAddress::LocalHost, 17655, &error))
            QSKIP("Default port is already occupied; leave the existing player untouched.");
        MediaController controller;
        configure(controller);
        track(controller);
        QSignalSpy launched(&controller, &MediaController::localPlayerStarted);
        controller.connectToPlayer({"127.0.0.1", 17655, {}});
        QTRY_VERIFY(controller.isReady());
        QCOMPARE(launched.size(), 0);
        QVERIFY(Settings().playerConnection().token.isEmpty());
        QVERIFY(!QFileInfo::exists(QDir(managedDirectory(17655)).filePath("control.token")));
    }

    void unusableStandaloneTokenStillAttemptsConnection()
    {
        QVERIFY(QDir().mkpath(playerDirectory()));
        // A directory is unreadable as a token on both Windows and Unix.
        QVERIFY(QDir().mkpath(QDir(playerDirectory()).filePath("control.token")));
        MediaBox::ControlServer peer(stoppedReply, QByteArray(64, 'b'));
        QString error;
        if (!peer.listen(QHostAddress::LocalHost, 17655, &error))
            QSKIP("Default port is already occupied; leave the existing player untouched.");
        MediaController controller;
        configure(controller);
        track(controller);
        QSignalSpy launched(&controller, &MediaController::localPlayerStarted);
        controller.connectToPlayer({"127.0.0.1", 17655, {}});
        QTRY_COMPARE(controller.connectionState(), State::AuthenticationFailed);
        QCOMPARE(launched.size(), 0);
        QVERIFY(QFileInfo::exists(QDir(managedDirectory(17655)).filePath("control.token")));
    }
};

QTEST_GUILESS_MAIN(PlayerAutostartTests)
#include "tst_playerautostart.moc"
