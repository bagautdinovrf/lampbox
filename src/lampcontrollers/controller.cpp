#include "controller.h"

Controller::Controller( QObject *parent) :
    QObject(parent)
{

}

bool Controller::Init(TracksFullInfo tracksFullInfo, QString &lErrorMsg, bool dbg){
    //Балансировщик, отлаживаемся
    Balance balancer(dbg);
    mTracksFullInfo = tracksFullInfo;
    TracksFullInfo::iterator it;

    //Контейнер для хранения треков с частотой
    TracksFreq NameFrequency;
    //Контейнер для хранения треков с установленной минутой запуска
    preferedMinute NamePreferedMinute;
    #ifdef SHOW_DEBUG_OUTPUT
    qDebug()<<tr("Controller::Init> полученное tracksFullInfo");
    #endif

    for( it = tracksFullInfo.begin(); it!=tracksFullInfo.end(); ++it ){
        #ifdef SHOW_DEBUG_OUTPUT
        it->ShowMe();
        #endif
        QString trackIdentity  = it->GetFileName();

        QString value = it->GetFrequency();
        int mIndex = value.indexOf("m");
        qDebug()<<"str:"<<value<<" INDEX OF M"<<mIndex;
        if (mIndex!=-1){
            QString prefM;
            if (mIndex == 1){
                prefM=value.left(1);

            }
            else{
                prefM=value.left(2);
            }

            int preferedMinute = prefM.toInt();

            NamePreferedMinute.insert( pair<int, QString>(preferedMinute,trackIdentity)) ;
        }
        else{
            NameFrequency.insert(pair<QString, int>(trackIdentity, value.toInt()));
        }


    }
    bool lStatus;

    if(NameFrequency.size()==0 && NamePreferedMinute.size()==0){
        lErrorMsg += tr("Controller::Init> Нет треков с установленной частотой и нет треков с предустановленными минутами запуска");
        mUserInfo =  tr("Пустой список рекламных роликов.");
        return false;
    }

    //Вектор в виде списка имен треков расставленных по порядку запуска
    idTracks trackList;
    //Если есть треки для которых указана частота
    if (NameFrequency.size()){
        trackList = balancer.MakeBalanced( NameFrequency, lStatus );
        if(!lStatus ){
           lErrorMsg+= tr("Не удалось создать поминутное расписание\n");
           mUserInfo = tr("Не удалось расставить треки с указанной частотой запуска по минутам.");
           return false;
         }
    }
    //Приделаем к списку trackList минуты
    if (! balancer.MakeStartMinutes( trackList, mTrackAndMinutesList,  NamePreferedMinute) ){

        lErrorMsg+=tr("Controller::Init > Не удалось расставить треки по минутам\n");
        mUserInfo=tr("Не удалось расставить треки по минутам.");
        return false;
    }
    //Установим заране
    if( mTrackAndMinutesList.size() == 0){
        lErrorMsg = tr("Controller::Init > Пустой список mTrackAndMinutesList\n");
        mUserInfo=tr("Не удалось разместить рекламные треки.");
        return false;
    }
return true;
}

bool Controller::GenerateCronList( QString& lErrorMsg ){
    //Заполним список минут запуска у объекта TrackFullInfo
    for(TracksFullInfo::iterator it =  mTracksFullInfo.begin(); it != mTracksFullInfo.end(); it++){
        //пробегаем по всему списку [трэк:минута]
        for( minuteOfTrack::iterator itr = mTrackAndMinutesList.begin(); itr!=mTrackAndMinutesList.end(); itr++) {
            //   если имя трека == имени трека в списке минута трек
            if ( (*itr).second == (*it).GetFileName() ){
                               (*it).AddMinute( (*itr).first);
            }
         }
    }
    #ifdef SHOW_DEBUG_OUTPUT
    for(TracksFullInfo::iterator it = mTracksFullInfo.begin(); it != mTracksFullInfo.end(); it++){
        (*it).ShowMe();
    }
    #endif
    for(TracksFullInfo::iterator it = mTracksFullInfo.begin(); it != mTracksFullInfo.end(); it++){
        QDate startDate = QDate::fromString((*it).mStartDate, "yyyy-MM-dd");
        QDate finishedDate = QDate::fromString((*it).mFinishedDate, "yyyy-MM-dd");
        if(startDate>finishedDate) {
            lErrorMsg += tr("В одной из записей начальный период больше конечного\n");
            return false;
        }
        MonthAndDaysList mAndD = GetDates(startDate, finishedDate, (*it).mTrackDayOfWeek);
        MonthAndDaysList::iterator itm;
        //Для всех месяцев создадим отдельные блоки в расписании
        for(itm = mAndD.begin(); itm!=mAndD.end(); itm++ ){

            CronItemData myCronItem((*it).GetFileName(),
                                    (*it).mMinutesList ,
                                    (*it).mTrackHours  ,
                                    (*itm).mDays,
                                    (*itm).mMonth,
                                    (((*it).mStartDate!="*") && ((*it).mFinishedDate!="*")? "*":(*it).mTrackDayOfWeek),
                                    (*it).mVolume);
            mCronList.push_back( myCronItem );
        }
    }

    //Покажем результат
    #ifdef SHOW_DEBUG_OUTPUT
        ShowCronList( mCronList );
    #endif
    if ( !CheckCronList(mCronList) ){
        lErrorMsg = tr("Есть пустые элементы в расписании рекламы.\n");
        lErrorMsg = tr("Возможно заданы невыполнимые условия.\n");
        return false;
    }
    return true;
}

minuteOfTrack Controller::GetTrackAndMinutesList() const
{
    return mTrackAndMinutesList;
}

MonthAndDaysList Controller::GetDates(
        QDate startDate,
        QDate finishedDate,
        QString dayOfWeek_) {
    /// \note   Исключения из обработки для обхода проблемы с кроном
    /// \bug    #7 Генерация cron файла.

    MonthAndDaysList  monthAndDaysList;

    enum {EQUAL_MONTH=0, NEIGHBOR_MONTH=1, ONE_MONTH_BETWEEN=2};

    QString monthList,monthStart,monthEnd,midleMonth;
    QString daysList, daysStart, daysEnd,midleDays;
    switch((finishedDate.month() - startDate.month())){
        case EQUAL_MONTH:
            monthList=QString("%1").arg(startDate.month());
            //qDebug()<<AllTimesByDayOfWeek(startDate,finishedDate,dayOfWeek_);

            if(dayOfWeek_!= QString("*")){
                daysList = AllTimesByDayOfWeek(startDate,finishedDate,dayOfWeek_);
            }else{
                daysList = QString("%1-%2").arg(startDate.day()).arg(finishedDate.day());
            }
            monthAndDaysList.push_back(MonthAndDays(monthList, daysList));
            break;

        case NEIGHBOR_MONTH:
            monthStart = QString("%1").arg(startDate.month());
            monthEnd = QString("%2").arg(finishedDate.month());
            if(dayOfWeek_!= QString("*")){
                daysStart = AllTimesByDayOfWeek(startDate,startDate.addDays(startDate.daysInMonth() - startDate.day()), dayOfWeek_);
                QDate finishedDateMonthBegin = finishedDate;
                finishedDateMonthBegin.setDate(finishedDate.year(),finishedDate.month(),1);
                daysEnd =AllTimesByDayOfWeek(finishedDateMonthBegin, finishedDate, dayOfWeek_);
            }else{
                daysStart =QString("%1-%2").arg(startDate.day()).arg(startDate.daysInMonth());
                daysEnd =QString("1-%1").arg( finishedDate.day());
            }
            monthAndDaysList.push_back( MonthAndDays( monthStart, daysStart ));
            monthAndDaysList.push_back( MonthAndDays( monthEnd, daysEnd ));
            break;

       case ONE_MONTH_BETWEEN:
            monthStart = QString("%1").arg(startDate.month());
            monthEnd = QString("%1").arg(finishedDate.month());
            midleMonth = QString("%1").arg(startDate.month()+1);

            if(dayOfWeek_!= QString("*")){
                daysStart = AllTimesByDayOfWeek(startDate,startDate.addDays(startDate.daysInMonth() - startDate.day()), dayOfWeek_);
                QDate finishedDateMonthBegin = finishedDate;
                QDate midleDaysMonthBegin(startDate.year(),startDate.month()+1,1);
                QDate midleDaysMonthEnd = midleDaysMonthBegin.addDays(midleDaysMonthBegin.daysInMonth()-1);

                finishedDateMonthBegin.setDate(finishedDate.year(),finishedDate.month(),1);
                daysEnd =AllTimesByDayOfWeek(finishedDateMonthBegin, finishedDate, dayOfWeek_);
                midleDays = AllTimesByDayOfWeek(midleDaysMonthBegin,midleDaysMonthEnd, dayOfWeek_);
            }else{
                daysStart =QString("%1-%2").arg(startDate.day()).arg(startDate.daysInMonth());
                daysEnd =QString("1-%1").arg( finishedDate.day());
                midleDays = QString("*");
            }

            monthAndDaysList.push_back( MonthAndDays( monthStart, daysStart ));
            monthAndDaysList.push_back( MonthAndDays( midleMonth, midleDays ));
            monthAndDaysList.push_back( MonthAndDays( monthEnd, daysEnd ));
            break;
    default:
            monthStart = QString("%1").arg(startDate.month());
            monthEnd = QString("%2").arg(finishedDate.month());

            if(dayOfWeek_!= QString("*")){
                daysStart = AllTimesByDayOfWeek(startDate,startDate.addDays(startDate.daysInMonth() - startDate.day()), dayOfWeek_);
                QDate finishedDateMonthBegin = finishedDate;
                finishedDateMonthBegin.setDate(finishedDate.year(),finishedDate.month(),1);
                daysEnd =AllTimesByDayOfWeek(finishedDateMonthBegin, finishedDate, dayOfWeek_);

                monthAndDaysList.push_back( MonthAndDays( monthStart, daysStart ));

                QDate currentMonth(startDate.year(),startDate.month()+1,1);
                QDate currentMonthEnd;
                for(; currentMonth.month()<finishedDate.month(); currentMonth = currentMonth.addMonths(1)){
                    currentMonthEnd = currentMonth.addDays(currentMonth.daysInMonth()-1);

                    midleDays = AllTimesByDayOfWeek(currentMonth,currentMonthEnd, dayOfWeek_);

                    monthAndDaysList.push_back( MonthAndDays(QString("%1").arg( currentMonth.month() ), midleDays ));
                }

                monthAndDaysList.push_back( MonthAndDays( monthEnd, daysEnd ));
            }else{
                daysStart =QString("%1-%2").arg(startDate.day()).arg(startDate.daysInMonth());
                daysEnd =QString("1-%1").arg( finishedDate.day());

                midleMonth = QString("%1-%2").arg(startDate.month()+1).arg(finishedDate.month()-1);
                midleDays = QString("*");

                monthAndDaysList.push_back( MonthAndDays( monthStart, daysStart ));
                monthAndDaysList.push_back( MonthAndDays( midleMonth, midleDays ));
                monthAndDaysList.push_back( MonthAndDays( monthEnd, daysEnd ));
            }
            break;
}
return monthAndDaysList;
}

QString Controller::AllTimesByDayOfWeek(QDate _beginDate, QDate _endDate, QString dayOfWeek){
    DaysOfWeek listDOW;
    QStringList slistDOW = dayOfWeek.split(",");
    QString cdow;

    if(slistDOW.empty())
        return QString();

    foreach(cdow,slistDOW){
        listDOW.push_back(Qt::DayOfWeek(cdow.toInt()));
    }

    Days days =  AllTimesByDayOfWeek(_beginDate,_endDate,listDOW);
    int i;
    QString result;
    foreach(i, days){
        result+=QString("%1,").arg(i);
    }
    if(!result.isEmpty())
        result.remove(result.count()-1,1);
    return result;
}

Days Controller::AllTimesByDayOfWeek(QDate _beginDate, QDate _endDate, DaysOfWeek dayOfWeek){

    if((_beginDate.month() - _endDate.month()!=0) || (_beginDate.year() - _endDate.year()!=0))
        return Days();

    if(dayOfWeek.empty())
        return Days();

    Days result;
    for(;_beginDate<=_endDate;_beginDate = _beginDate.addDays(1)){
            Qt::DayOfWeek d;
            foreach(d,dayOfWeek){
                 if(d == _beginDate.dayOfWeek())
                    result.push_back(_beginDate.day());
            }
    }

    return result;
}

void Controller::ShowCronList(CronList cronList){
    foreach( CronItemData cronItemData_,cronList ){
        cronItemData_.ShowMe( );
    }
}

bool Controller::CheckCronList(CronList cronList ){
    foreach(CronItemData cronItemData_, cronList){
        if ( cronItemData_.IsEmptyElement() ) return false;
    }
return true;
}
