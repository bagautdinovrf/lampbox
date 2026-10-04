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
#include "storagepaths.h"

#ifdef Q_OS_ANDROID
#include <QtCore/private/qandroidextras_p.h>
#include <QScopeGuard>
#include "androidservice.h"
#endif

namespace {
#ifdef Q_OS_WIN
bool hasStartupFlag(int argc, char *argv[], const QByteArray &flag)
{
    for (int i = 1; i < argc; ++i) {
        const QByteArray argument(argv[i]);
        if (argument == "--")
            break;
        if (argument == flag)
            return true;
        // Values belong to the preceding option, even if they look like flags.
        if (argument == "--data-dir" || argument == "--listen" || argument == "--port")
            ++i;
    }
    return false;
}
#endif

QStringList legacyControlTokens()
{
    QStringList paths;
#ifdef Q_OS_WIN
    paths.append(QDir(MediaBox::StoragePaths::commonConfigurationDirectory())
                     .filePath(QStringLiteral("Player/control.token")));
#endif
    paths.append(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                     .filePath(QStringLiteral("control.token")));
    return paths;
}

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
    const QString defaultDataDirectory = MediaBox::StoragePaths::configurationDirectory(
        MediaBox::StoragePaths::Application::Player);
    parser.addOption({QStringLiteral("data-dir"),
                      QStringLiteral("Каталог блокировки процесса и токена управления (по умолчанию: %1).")
                          .arg(defaultDataDirectory),
                      QStringLiteral("path"), defaultDataDirectory});
    parser.addOption({QStringLiteral("listen"), QStringLiteral("IP-адрес API управления."),
                      QStringLiteral("address"), QStringLiteral("127.0.0.1")});
    parser.addOption({QStringLiteral("port"), QStringLiteral("TCP-порт API управления."),
                      QStringLiteral("port"), QStringLiteral("17655")});
#ifndef Q_OS_ANDROID
    parser.addOption({QStringLiteral("managed"),
                      QStringLiteral("Фоновый запуск из Manager: явный --data-dir, loopback API и токен из MEDIABOXPLAYER_CONTROL_TOKEN.")});
#endif
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

    QByteArray token;
#ifndef Q_OS_ANDROID
    const bool managed = parser.isSet(QStringLiteral("managed"));
    if (managed) {
        token = qgetenv("MEDIABOXPLAYER_CONTROL_TOKEN");
        qunsetenv("MEDIABOXPLAYER_CONTROL_TOKEN");
        if (!parser.isSet(QStringLiteral("data-dir")) || !address.isLoopback()) {
            qCritical() << "Managed mode requires an explicit --data-dir and a loopback --listen address.";
            return 2;
        }
        bool validToken = token.size() == 64;
        for (const char byte : token)
            validToken = validToken && ((byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f'));
        if (!validToken) {
            qCritical() << "Managed mode requires MEDIABOXPLAYER_CONTROL_TOKEN with 64 lowercase hexadecimal characters.";
            return 2;
        }
    }
#endif

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
    const QStringList legacyTokens = parser.isSet(QStringLiteral("data-dir"))
        ? QStringList() : legacyControlTokens();
    // A managed token belongs to the launching Manager. Do not replace or create
    // the standalone/service token, which can have a different lifetime.
    if (token.isEmpty() && !MediaBox::loadControlToken(dataDirectory, &token, &error, legacyTokens)) {
        qCritical().noquote() << error;
        return 1;
    }
    MediaBox::DesktopServiceLifecycle lifecycle;
    if (!lifecycle.install(&error)) {
        qCritical().noquote() << error;
        return 1;
    }

    MediaBox::QtAudioBackend backend;
    MediaBox::PlayerEngine engine(&backend, nullptr, [] { return QDateTime::currentDateTime(); },
                                  QDir(dataDirectory).filePath(QStringLiteral("runtime.sqlite")));
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
        // Service shutdown preserves the requested schedule mode for startup;
        // an explicit user stop command persists manual mode separately.
        engine.setPlaybackAvailable(false);
    });

    if (!lifecycle.notifyReady())
        return 0;
    const QString restoreError = engine.restoreScheduledPlayback();
    if (!restoreError.isEmpty())
        qWarning().noquote() << "Cannot restore scheduled playback:" << restoreError;
    qInfo().noquote() << "MediaBoxPlayer ready; API:" << address.toString() << server.port()
                      << "; data directory:" << dataDirectory;
    return application.exec();
}
} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (hasStartupFlag(argc, argv, "--service")) {
        if (hasStartupFlag(argc, argv, "--managed")) {
            qCritical() << "--managed and --service cannot be used together.";
            return 2;
        }
        return MediaBox::runWindowsService(argc, argv, runPlayer);
    }
#endif
    return runPlayer(argc, argv);
}
