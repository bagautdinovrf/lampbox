 /**
    @file    Генерирует список строк cron файла, так называемую рекламную сетку
    @author  Климов А.В.
*/

#ifndef CONTROLLER_H
#define CONTROLLER_H
#include <QObject>
#include <QDateEdit>
#include <QString>

//#include <QLMCommons>
//#include <QLMObjects>
#include "trackfullinfo.h"
#include "cron.h"
#include "balance.h"
//using namespace lampproject;
//using namespace lampproject::lmobject;


class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(QObject *parent = 0);
    //! @fn Функция создает расписание по минутам, если не удалось то false
    bool Init(TracksFullInfo, QString& lErrorMsg, bool dbg);

    //! @fn GenerateCron создает cron List
    //! @var errMsg - сообщение возвращаемое в случае ошибки
    //! @ret код возврата OK=true ERROR=false
    bool GenerateCronList(QString &errMsg);

    MonthAndDaysList GetDates(QDate startDate, QDate finishedDate, QString dayOfWeek);

    //! @fn GetTrackAndMinutesList возвращает
    //!
    minuteOfTrack GetTrackAndMinutesList() const;

    CronList mCronList;
    //Сообщения для пользователя
    QString mUserInfo;
private:
      TracksFullInfo mTracksFullInfo;
      minuteOfTrack mTrackAndMinutesList;
      /// \brief Функция выбирает все числа интервале ОДНОГО месяца
      ///        которые выподают на указанные дни недели
      ///        Преобразует строку к виду DaysOfWeek
      ///        Вызывает @see AllTimesByDayOfWeek(QDate,QDate,DaysOfWeek dayOfWeek);
      QString AllTimesByDayOfWeek(QDate,QDate,QString dayOfWeek);
      Days AllTimesByDayOfWeek(QDate,QDate,DaysOfWeek dayOfWeek);
      //!
      void ShowCronList(CronList);
      //! Проверяем нет ли пустых элементов в CronList
      bool CheckCronList(CronList );
signals:

public slots:

};

#endif // CONTROLLER_H
