#include "controlserver.h"
#include "desktopservice.h"
#include "storagepaths.h"
#include "videoservice.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QHostAddress>
#include <QLockFile>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    QCoreApplication::setApplicationName(QStringLiteral("MediaBoxVPlayer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(MEDIABOXVPLAYER_VERSION));
    qSetMessagePattern(QStringLiteral("[%{time yyyy-MM-dd hh:mm:ss.zzz}] %{type}: %{message}"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Многооконный видеоплеер MediaBoxVPlayer. Управление: JSON/TCP API v1."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("host"), QStringLiteral("IP-адрес TCP API управления."),
                      QStringLiteral("address"), QStringLiteral("127.0.0.1")});
    parser.addOption({QStringLiteral("port"), QStringLiteral("TCP-порт API управления."),
                      QStringLiteral("port"), QStringLiteral("17656")});
    const QString defaultDirectory = MediaBox::StoragePaths::configurationDirectory(
        MediaBox::StoragePaths::Application::VideoPlayer);
    parser.addOption({QStringLiteral("data-dir"), QStringLiteral("Каталог настроек, токена и блокировки процесса."),
                      QStringLiteral("path"), defaultDirectory});
    parser.process(application);
    if (!parser.positionalArguments().isEmpty())
        parser.showHelp(2);
    bool validPort = false;
    const uint port = parser.value(QStringLiteral("port")).toUInt(&validPort);
    QHostAddress address;
    if (!validPort || port == 0 || port > 65535 || !address.setAddress(parser.value(QStringLiteral("host")))) {
        qCritical() << "Invalid --host IP address or --port (1..65535).";
        return 2;
    }
    const QString dataDirectory = QDir(parser.value(QStringLiteral("data-dir"))).absolutePath();
    if (parser.value(QStringLiteral("data-dir")).isEmpty() || !QDir().mkpath(dataDirectory)) {
        qCritical() << "Cannot create video player data directory:" << dataDirectory;
        return 1;
    }
    QLockFile lock(QDir(dataDirectory).filePath(QStringLiteral("player.lock")));
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) {
        qCritical() << "Video player data directory is already in use or cannot be locked:" << dataDirectory;
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
    MediaBox::VideoService service(dataDirectory);
    if (!service.restore(&error)) {
        qCritical().noquote() << "Cannot restore video windows:" << error;
        return 1;
    }
    MediaBox::ControlServer server([&service](const QJsonObject &request) { return service.execute(request); }, token);
    if (!server.listen(address, static_cast<quint16>(port), &error)) {
        qCritical().noquote() << "Cannot listen for Manager commands:" << error;
        return 1;
    }
    if (!lifecycle.notifyReady())
        return 0;
    qInfo().noquote() << "MediaBoxVPlayer ready; API:" << address.toString() << server.port()
                      << "; data directory:" << dataDirectory;
    return application.exec();
}
