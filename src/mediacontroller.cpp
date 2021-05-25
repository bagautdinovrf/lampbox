
#include "mediacontroller.h"
#include "lampdata.h"


#include <QProcess>
#include <QDebug>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>


MediaController::MediaController(QObject *parent) :
    QObject(parent)
{
    QTimer *playing = new QTimer(this);
    connect( playing, SIGNAL(timeout()), this, SLOT(sendPlayingState() ) );
    // Проверка работы проигрывания музыки раз в 3 секунды
    playing->setInterval(3000);
    playing->start();
}


MediaController::~MediaController()
{
    //
}

/**
 * @brief play              - Запуск плеера
 * @return
 */
bool MediaController::play()
{
    SPathData p;
#ifdef Q_OS_LINUX
    if(!startProcess("service cron start") )
        return false;
    if( !QProcess::startDetached( /*p.homePathDir+p.scriptsDir+*/p.playMusicFile ) )
        return false;
#elif defined(Q_OS_WIN)
//    if(!startProcess("sc start cron") )
//        return false;
//    if( !media.startDetached("wperl.exe \"" + p.playMusicFile + "\"") )
//        return false;
    if( !QProcess::startDetached(p.playMusicFile, QStringList() << "play") )
        return false;
#endif
    emit playingState( true );
    return true;
}

/**
 * @brief stop              - Остановка плеера
 * @return
 */
bool MediaController::stop()
{
#ifdef Q_OS_LINUX
    if(!startProcess("service cron stop") )
        return false;
    if(!startProcess("killall mplayer" ) )
        return false;
#elif defined(Q_OS_WIN)
//    if(!startProcess("sc stop cron") )
//        return false;
//    if(!startProcess("taskkill.exe /im perl.exe /f" ) )
//        return false;
//    if(!startProcess("taskkill.exe /im wperl.exe /f" ) )
//        return false;
//    if(!startProcess("taskkill.exe /im mpg123.exe /f" ) )
//        return false;
    SPathData p;
    if(! QProcess::startDetached(p.playMusicFile, QStringList() << "stop") )
        return false;
#endif
    emit playingState( false );
    return true;
}

/**
 * @brief isPlaying         - Состояние плеера
 * @return
 */
bool MediaController::isPlaying()
{
    QProcess cron;
#ifdef Q_OS_LINUX
    cron.start( "service cron status" );
    if (!cron.waitForStarted())
        return false;
    if (!cron.waitForFinished())
        return false;
    QString qs = QString( cron.readAll() );
    if( qs.contains( "start/running" ) )
        return true;
    else return false;
#elif defined(Q_OS_WIN)

    QString configFile = StationManager::Instance().configFile();
//    QFile file(configFile);
//    if( !file.exists() )
//        return false;
    QSettings settings(configFile, QSettings::IniFormat );
    int isPlay = settings.value( "stop", 1 ).toInt();
    if( 0 == isPlay )
        return true;
    else return false;
#endif
    return false;
}

/**
 * @brief sendPlayingState  - Отправка текущего состояния плеера
 */
void MediaController::sendPlayingState()
{
    emit playingState( isPlaying() );
}

/**
 * @brief startProcess      - Запуск процесса
 * @param arg           - строка для запуска
 * @return      true    - успешно
 *              false   - не удалось запустить
 */
bool MediaController::startProcess(const QString arg )
{
     QProcess proc;
     proc.start( arg );
     if (!proc.waitForStarted())
         return false;
     if (!proc.waitForFinished())
         return false;
return true;
}

/**
 * @brief showPlayer        - Показ интерфейса плеера
 */
bool MediaController::showPlayer()
{
    SPathData p;
    if(! QProcess::startDetached(p.playMusicFile, QStringList() << "show") )
        return false;
return true;
}

/**
 * @brief hide              - Cкрытие интерфейса плеера
 */
bool MediaController::hidePlayer()
{
    SPathData p;
    if(! QProcess::startDetached(p.playMusicFile, QStringList() << "hide") )
        return false;
//    if( !QDesktopServices::openUrl( QUrl::fromLocalFile(p.playMusicFile + " hide") ) )
//        return false;
return true;
}

void MediaController::refreshPlayer()
{
    SPathData p;
    QProcess::startDetached(p.playMusicFile, QStringList() << "refresh");
}

//void MediaController::ready()
//{
//    qDebug() << qobject_cast<QProcess*>(sender())->readAll();
//    qDebug() << qobject_cast<QProcess*>(sender())->readAllStandardOutput();
//    qobject_cast<QProcess*>(sender())->kill();
//    delete sender();
//}

/**
 * @brief slot_playerHide   - Cкрыть интерфейс плеера
 */
void MediaController::slot_playerHide()
{
    SPathData p;
    QProcess::startDetached(p.playMusicFile, QStringList() << "hide");
}

/**
 * @brief slot_playerHide   - Показать интерфейс плеера
 */
void MediaController::slot_playerShow()
{
    SPathData p;
    QProcess::startDetached(p.playMusicFile, QStringList() << "show");
}


void MediaController::playTrack(const QString &track )
{
    QProcess::startDetached(SPathData().playMusicFile, QStringList() << track);
}
