
#include "advertdata.h"


AdvertData::AdvertData()
{
    //
}


void AdvertData::setName(const QString &name)
{
    mName = name;
}

void AdvertData::setDays(const QString &days)
{
    mDayOfWeek = days;
}

void AdvertData::setEndDate(QDate endDate)
{
    mEndDate = endDate;
}

void AdvertData::setHours(const QString &hours)
{
    mHours = hours;
}

void AdvertData::setMinuts(const QString &minuts)
{
    mMinuts = minuts;
}

void AdvertData::setStartDate(QDate startDate)
{
    mStartDate = startDate;
}

void AdvertData::setVolume(int volume)
{
    mVolume = volume;
}

QString AdvertData::name() const
{
    return mName;
}

QString AdvertData::days() const
{
    return mDayOfWeek;
}

QDate AdvertData::endDate() const
{
    return mEndDate;
}

QDate AdvertData::startDate() const
{
    return mStartDate;
}

QString AdvertData::hours() const
{
    return mHours;
}

QString AdvertData::minuts() const
{
    return mMinuts;
}

int AdvertData::volume() const
{
    return mVolume;
}
