

#ifndef LAMPDATA_H
#define LAMPDATA_H
#include "settings.h"
#include "stationmanager.h"
#include <QString>
#include <QFileInfo>
#include <QStringList>
#include <QDir>

namespace {
    QString LAMP_DATE_FORMAT = "dd.MM.yyyy";
}

namespace LampBox {
    enum CHANNEL_TYPE { NO_TYPE, MUSIC, VIDEO, ADVERT };
}

struct SPathData
{

    SPathData()
    {
        StationManager::Instance().update();
        homePathDir         = STATIONPATH;                                    /// Домашняя директорияы
        channelDirMusic     = STATIONMEDIATO("music/");                       /// Директория с каналами
        channelDirVideo     = STATIONMEDIATO("video/");                       /// Директория с каналами
        sheduleDir          = STATIONPATHTO("timetable/");                    /// Директория с файлами расписания
        sheduleFileMusic    = STATIONPATHTO("timetable/timetable");           /// Файл расписания музыки
        sheduleFileVideo    = STATIONPATHTO("timetable/vtimetable");          /// Файл расписания видео
#ifdef Q_OS_WIN
        cronPath = STATIONPATHTO("nncronlt/");
        advertTask = cronPath + "cron.tab";                             /// cron файл рекламы
        playMusicFile = STATIONPATHTO("lmplayer.exe");                  /// Плеер запуска музыки
        mediaPath = "";                                                 /// Директория автомонтирования флешки
#elif defined(Q_OS_LINUX)
        cronPath = "/etc/cron.d/";
        advertTask = cronPath + "rektask";                              /// cron файл рекламы
        playMusicFile = STATIONPATHTO("scripts/play_music.pl");         /// Скрипт запуска музыки
        mediaPath = "/media";                                           /// Директория автомонтирования флешки
#endif
        advertTableFile = STATIONPATHTO( "timetable/adverttable");      /// Файл с расписанием рекламы в csv
        advertViewFile = STATIONPATHTO( "timetable/advertView");        /// Файл с расписанием рекламы в csv для представления
        advertDir = STATIONMEDIATO("ads/");                             /// Директория с рекламным контентом
        mediaPlayerConfig = StationManager::Instance().configFile();
    }


    /// Рабочий каталог
    QString                                       homePathDir;

    /// Каталог с каналами
    QString                                       channelDirMusic;

    /// Видео каналы
    QString                                       channelDirVideo;

    /// Каталог с расписанием
    QString                                       sheduleDir;

    /// Файл c расписанием
    QString                                       sheduleFileMusic;

    /// Файл c расписанием видео
    QString                                       sheduleFileVideo;

    ///
    QString                                       playMusicFile;

    /// Директория с расписанием крона
    QString                                       cronPath;

    /// Файл с расписанием рекламы
    QString                                       advertTask;

    /// Файл с расписанием рекламы для таблицы
    QString                                       advertTableFile;
    QString                                       advertViewFile;

    /// Директория с рекламой
    QString                                       advertDir;

    /// Директория с медиаконтентом
    QString                                       mediaPath;

    /// Файл конфигурации
    QString                                       mediaPlayerConfig;

    /**
     * @brief addFiles  - Возвращает список файлов
     * @param paths     - список выделенных объектов
     * @return          - список файлов
     */
    static QStringList addFiles(const QStringList &paths )
    {
        QStringList fileList;
        QFileInfo fileInfo;
        for( int i = 0; i < paths.size(); ++i ) {
            fileInfo.setFile( paths.at(i) );
            if(  fileInfo.isFile() )
                fileList.append( paths.at(i) );
            else if(fileInfo.isDir() ) {
                QDir dir(paths.at(i));
                QStringList files = dir.entryList( Settings().availablelAllFileFormats(), QDir::Files, QDir::Name);
                for(int k = 0; k < files.size(); ++k )
                    fileList.append( dir.path() + "/" + files.at(k) );
            }
        }
        return fileList;
    }
};


#endif // LAMPDATA_H
