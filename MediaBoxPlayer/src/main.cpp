#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QHostAddress>
#include <QLockFile>
#include <QStandardPaths>

#include "controlserver.h"
#include "desktopservice.h"
#include "playerengine.h"
#include "qtaudiobackend.h"

#ifdef Q_OS_ANDROID
#include <QtCore/private/qandroidextras_p.h>
#include <QScopeGuard>
#include "androidservice.h"
#endif

namespace {
int runPlayer(int argc, char *argv[])
{
#ifdef Q_OS_ANDROID
    QAndroidService application(argc, argv);
    const auto serviceCleanup = qScopeGuard([] { MediaBox::AndroidService::stop(); });
#else
    QCoreApplication application(argc, argv);
#endif
    QCoreApplication::setApplicationName(QStringLiteral("MediaBoxPlayer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(MEDIABOXPLAYER_VERSION));
    qSetMessagePattern(QStringLiteral("[%{time yyyy-MM-dd hh:mm:ss.zzz}] %{type}: %{message}"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Фоновый аудиоплеер MediaBoxPlayer. Управление: JSON/TCP API v1."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("data-dir"),
                      QStringLiteral("Каталог блокировки процесса и токена управления."), QStringLiteral("path"),
                      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)});
    parser.addOption({QStringLiteral("listen"), QStringLiteral("IP-адрес API управления."),
                      QStringLiteral("address"), QStringLiteral("127.0.0.1")});
    parser.addOption({QStringLiteral("port"), QStringLiteral("TCP-порт API управления."),
                      QStringLiteral("port"), QStringLiteral("17655")});
#ifdef Q_OS_WIN
    parser.addOption({QStringLiteral("service"), QStringLiteral("Запуск службой Windows через SCM.")});
#endif
    parser.process(application);
    if (!parser.positionalArguments().isEmpty())
        parser.showHelp(2);

    bool validPort = false;
    const uint port = parser.value(QStringLiteral("port")).toUInt(&validPort);
    QHostAddress address;
    if (!validPort || port == 0 || port > 65535
        || !address.setAddress(parser.value(QStringLiteral("listen")))) {
        qCritical() << "Invalid --listen IP address or --port (1..65535).";
        return 2;
    }

    const QString dataDirectory = QDir(parser.value(QStringLiteral("data-dir"))).absolutePath();
    if (parser.value(QStringLiteral("data-dir")).isEmpty() || !QDir().mkpath(dataDirectory)) {
        qCritical() << "Cannot create player data directory:" << dataDirectory;
        return 1;
    }
    QLockFile processLock(QDir(dataDirectory).filePath(QStringLiteral("player.lock")));
    processLock.setStaleLockTime(0);
    if (!processLock.tryLock()) {
        qCritical() << "Player data directory is already in use or cannot be locked:" << dataDirectory;
        return 1;
    }

    QString error;
    QByteArray token;
    if (!MediaBox::loadControlToken(dataDirectory, &token, &error)) {
        qCritical().noquote() << error;
        return 1;
    }
    MediaBox::DesktopServiceLifecycle lifecycle;
    if (!lifecycle.install(&error)) {
        qCritical().noquote() << error;
        return 1;
    }

    MediaBox::QtAudioBackend backend;
    MediaBox::PlayerEngine engine(&backend);
    MediaBox::ControlServer server(&engine, token);
    if (!server.listen(address, static_cast<quint16>(port), &error)) {
        qCritical().noquote() << "Cannot listen for Manager commands:" << error;
        return 1;
    }
    QString lastState;
    QString lastError;
    QObject::connect(&engine, &MediaBox::PlayerEngine::statusChanged, &application, [&] {
        const auto status = engine.status();
        const QString state = status.value(QStringLiteral("state")).toString();
        const QString playbackError = status.value(QStringLiteral("error")).toString();
        if (state != lastState) {
            qInfo().noquote() << "Playback state:" << state;
            lastState = state;
        }
#ifdef Q_OS_ANDROID
        // Keep the CPU awake through decoding and transitions between tracks,
        // while state itself continues to report only backend-confirmed playback.
        MediaBox::AndroidService::setPlaying(status.value(QStringLiteral("playbackRequested")).toBool());
#endif
        if (playbackError != lastError) {
            if (!playbackError.isEmpty())
                qWarning().noquote() << playbackError;
            lastError = playbackError;
        }
    });
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &application, [&] {
        engine.execute({{QStringLiteral("command"), QStringLiteral("stop")}});
    });

    if (!lifecycle.notifyReady())
        return 0;
    qInfo().noquote() << "MediaBoxPlayer ready; API:" << address.toString() << server.port()
                      << "; data directory:" << dataDirectory;
    return application.exec();
}
} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    for (int i = 1; i < argc; ++i) {
        if (QByteArray(argv[i]) == "--service")
            return MediaBox::runWindowsService(argc, argv, runPlayer);
    }
#endif
    return runPlayer(argc, argv);
}
