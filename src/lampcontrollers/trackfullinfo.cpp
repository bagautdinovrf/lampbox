#include "trackfullinfo.h"
#include <QFileInfo>

TrackFullInfo::TrackFullInfo(QString TrackName,
    QString Frequency,
    QString Hours,
    QString DayOfWeek,
    QString StartDate,
    QString FinishedDate,
    QString FileName,
    QString Volume,
    QString TrackStatus,
    QString CronTrackStatus,
    QString TrackId
    , QString order) :
    mTrackName( TrackName ),
    mTrackFrequency( Frequency ),
    mTrackHours( Hours ),
    mTrackDayOfWeek( DayOfWeek ),
    mStartDate( StartDate ),
    mFinishedDate( FinishedDate ),
    mFileName(FileName),
    mTrackStatus(TrackStatus),
    mCronTrackStatus(CronTrackStatus),
    mTrackId(TrackId),
    mVolume(Volume),
    mOrder(order)
{
    mMinutesList = QString("");
}

TrackFullInfo::TrackFullInfo()
{
    //
}


QString TrackFullInfo::GetName() const  {
    return mTrackName;
}

QString TrackFullInfo::GetFileName() const  {
    return QFileInfo(mFileName).fileName();
}

QString TrackFullInfo::GetFrequency() const {
    return mTrackFrequency;
    //return mTrackFrequency.toInt();
}

QString TrackFullInfo::GetHours() const {
    return mTrackHours;
}

void TrackFullInfo::AddMinute(int minute)
{
    AddMinute(QString::number(minute));
}

void TrackFullInfo::AddMinute(const QString &minute)
{
//    if( minute.isEmpty() )
//        return;
    if( !mMinutesList.isEmpty() )
        mMinutesList.append(',');
    mMinutesList.append( minute );
}

QString TrackFullInfo::GetDayOfWeek() const{

    return mTrackDayOfWeek;
}

QString TrackFullInfo::GetTrackId() const{

    return mTrackId;
}

QString TrackFullInfo::GetStatus() const{

    return mTrackStatus;
}

void TrackFullInfo::ShowMe()
{
    qDebug() << "mTrackName: " << mTrackName;
    qDebug() << "mTrackFrequency : " << mTrackFrequency;
    qDebug() << "mTrachHours : " << mTrackHours;
    qDebug() << "mTrackDayOfWeek : " << mTrackDayOfWeek;
    qDebug() << "mTrackStartDate : " << mStartDate;
    qDebug() << "mTrackFinishedDate : " << mFinishedDate;
    qDebug() << "mFileName : " << mFileName;
    qDebug() << "mTrackStatus: " << mTrackStatus;
    qDebug() << "mCronTrackStatus: " << mCronTrackStatus;
    qDebug() << "mTrackId: " << mTrackId;
    qDebug() << "Volume: " << mVolume;
}

void TrackFullInfo::setMinuts(const QString &minuts)
{
    mMinutesList = minuts;
}

QString TrackFullInfo::startTime() const
{
    return mStartDate;
}

QString TrackFullInfo::endTime() const
{
    return mFinishedDate;
}

QString TrackFullInfo::minuts() const
{
    return mMinutesList;
}

QString TrackFullInfo::volume() const
{
    return mVolume;
}

QString TrackFullInfo::order() const
{
    return mOrder;
}
