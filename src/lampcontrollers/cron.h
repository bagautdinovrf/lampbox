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
#include <QList>
#include <vector>
#include <utility>
#include <QDebug>
//#define  STOP_STRING   QString("59 23 * * * root /home/scripts/killer.sh \n")
#define  START_STRING  QString("MAILTO=\"\"\n")
///home/scripts/playkanal.sh music \n")

struct CronItemData
{
    QString Filename; //Имя файла
    QString m;        //Месяц
    QString h;        //Час
    QString dom;      //День месяца
    QString mon;      //Месяц
    QString dow;      //День недели
    QString volume;   //Громокость

    void ShowMe() const {
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
    bool IsEmptyElement() const {
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

    CronItemData() = default;
    CronItemData(QString _Filename,
                  QString _m,
                  QString _h,
                  QString _dom,
                  QString _mon,
                  QString _dow,
                  QString _volume):
        Filename(std::move(_Filename)), m(std::move(_m)), h(std::move(_h)),
        dom(std::move(_dom)), mon(std::move(_mon)), dow(std::move(_dow)),
        volume(std::move(_volume))
    {
    }

};

struct MonthAndDays {
    QString mMonth;
    QString mDays;

    MonthAndDays(QString _month, QString _days)
        : mMonth(std::move(_month)), mDays(std::move(_days))
    {}
};

using CronList = QList<CronItemData>;
using MonthAndDaysList = std::vector<MonthAndDays>;
using Days = std::vector<int>;
using DaysOfWeek = std::vector<Qt::DayOfWeek>;

#endif // CRON_H
