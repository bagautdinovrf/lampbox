#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MediaBoxPlayer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(MEDIABOXPLAYER_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Каркас сервиса проигрывания музыки MediaBoxPlayer."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(application);

    qInfo().noquote() << QStringLiteral(
        "MediaBoxPlayer: процесс запущен; проигрывание музыки пока не реализовано.");
    return application.exec();
}
