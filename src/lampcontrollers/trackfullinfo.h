/*
 @class Класс описывает информацию о рекламном ролике и параметрах его запуска
 @author Климов А.В.
*/
#ifndef TRACKFULLINFO_H
#define TRACKFULLINFO_H

#include <vector>
#include <QString>
#include <QDebug>
//#include "common.h"

using namespace std;

class  TrackFullInfo;

typedef vector<TrackFullInfo> TracksFullInfo;

class  TrackFullInfo
{
public:
    TrackFullInfo();

    QString mTrackName;
    QString mTrackFrequency;
    QString mTrackHours;
    QString mTrackDayOfWeek;
    QString mMinutesList;
    QString mStartDate;
    QString mFinishedDate;
    QString mFileName;
    QString mTrackStatus;
    QString mCronTrackStatus;
    QString mTrackId;
    QString mTrackReservedMinute;
    QString mVolume;
    QString mOrder;

    TrackFullInfo(
                 QString trackName,
                 QString frequency,
                 QString trachHours,
                 QString trackDayOfWeek,
                 QString trackStartDate,
                 QString trackFinishedDate,
                 QString fileName,
                 QString volume,
                 QString trackStatus=QString(""),
                 QString cronTrackStatus = QString(""),
                 QString trackId = QString(""),
                 QString order = "0"
                );

    void ShowMe();




    //! @fn GetName Запрос имени трека
    //! @return возвращает имя трэка
    QString GetName() const;


    QString    GetFrequency() const;
    //! @fn GetFileName
    //! @return возвращает имя файла на сервере
    QString GetFileName() const;
    //! @fn Запрос максимальной частоты.
    //! @return Максимальная частота.
    QString GetHours() const;

    //! @fn GetHours Запрос строки содержащей часы запуска.
    //! @return строка содержащая часы, разделенные запятой.
    void AddMinute(int minute);
    void AddMinute(const QString &minute);

    //! @fn AddMinute Функция добавляет минуту к строке содержащей список минут.
    QString GetDayOfWeek() const;
    //! @fn GetDayOfWeek Функция запрашивает список дней недели, которые выставляются в таблице
    //! @return дни недели в заданном в таблице виде например 1-4 или 1,2,3

    //! @fn AddMinute Функция добавляет минуту к строке содержащей список минут.
    QString GetStatus() const;

    //! @fn GetTrackId() Функция запрашивает Id трэка(хранящийся в таблице IdTracks)
    //! @return IdTrack
    QString GetTrackId() const;

//    QString GetReservedMinute() const;

    void setMinuts(const QString &minuts);

    QString startTime() const;
    QString endTime() const;
    QString minuts() const;
    QString volume() const;

    QString order() const;
} ;

#endif // TRACKFULLINFO_H
