/**
Файл структруы cron файла
* * * * * выполняемая команда
- - - - -
| | | | |
| | | | ----- День недели (0 - 7) (Воскресенье =0 или =7)
| | | ------- Месяц (1 - 12)
| | --------- День (1 - 31)
| ----------- Час (0 - 23)
------------- Минута (0 - 59)
*/

#ifndef CRON_H
#define CRON_H
#include <QString>
#include <QMap>
#include <QVector>
#include <vector>
#include <QDebug>
//#define  STOP_STRING   QString("59 23 * * * root /home/scripts/killer.sh \n")
#define  START_STRING  QString("MAILTO=\"\"\n")
///home/scripts/playkanal.sh music \n")

typedef struct _CronItemData
{
    QString Filename; //Имя файла
    QString m;        //Месяц
    QString h;        //Час
    QString dom;      //День месяца
    QString mon;      //Месяц
    QString dow;      //День недели
    QString volume;   //Громокость

    void ShowMe(){
        qDebug()<<"------------------------";
        qDebug()<<"FileName : " << Filename;
        qDebug()<<"m        : " << m;
        qDebug()<<"h        : " << h;
        qDebug()<<"dom      : " << dom;
        qDebug()<<"mon      : " << mon;
        qDebug()<<"dow      : " << dow;
        qDebug()<<"volume      : " << volume;
        qDebug()<<"------------------------";
    }
    bool IsEmptyElement(){
        bool status = false;
        if ( Filename.isEmpty()) status = true;
        if ( m.isEmpty())       status = true;
        if ( h.isEmpty())       status = true;
        if ( dom.isEmpty())     status = true;
        if ( mon.isEmpty())     status = true;
        if (dow.isEmpty())      status = true;
        if(!status){
            qDebug()<<"Cron.h::isEmptyElement> Empty Element";
            ShowMe();
        }
        return status;
    }

    _CronItemData(){}
    _CronItemData(QString _Filename,
                  QString _m,
                  QString _h,
                  QString _dom,
                  QString _mon,
                  QString _dow,
                  QString _volume):
        Filename(_Filename), m(_m), h(_h), dom(_dom), mon(_mon), dow(_dow),volume(_volume)
    {
    }

} CronItemData;

typedef struct _MonthAndDays{
    QString mMonth;
    QString mDays;

    _MonthAndDays(QString _month, QString _days ):mMonth( _month ),mDays( _days )
    {}
} MonthAndDays;

typedef QVector<CronItemData> CronList;
typedef std::vector<MonthAndDays>  MonthAndDaysList;
typedef std::vector<int> Days;
typedef std::vector<Qt::DayOfWeek> DaysOfWeek;

#endif // CRON_H
