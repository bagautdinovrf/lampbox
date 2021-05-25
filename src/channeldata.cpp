
#include "channeldata.h"

namespace LampBox {

ChannelData::ChannelData(CHANNEL_TYPE type) :
    mMediaManager(type),
    mValid(true)
{
    //
}

ChannelData::ChannelData() :
    mMediaManager(NO_TYPE),
    mValid(false)
{

}


ChannelData::~ChannelData()
{
    //
}

bool ChannelData::isValid()
{
    return mValid;
}

////////////////////////////////////////////////////////
// SET - GET
////////////////////////////////////////////////////////
/**
 * @brief ChannelData::setChannelName
 * @param name
 */
void ChannelData::setChannelName( const QString& name )
{
    mChannelName = name;
    mMediaManager.setDirName(name);
}

/**
 * @brief ChannelData::setNum
 * @param id
 */
void ChannelData::setNum( int id )
{
    mId = id;
}

/**
 * @brief ChannelData::setStartTime
 * @param startTime
 */
void ChannelData::setStartTime( const QTime &startTime )
{
    mStartTime = startTime;
}

/**
 * @brief ChannelData::setEndTime
 * @param endTime
 */
void ChannelData::setEndTime(const QTime &endTime )
{
    mEndTime = endTime;
}

/**
 * @brief setDaysOfWeek
 * @param days
 */
void ChannelData::setDaysOfWeek( const QString &days)
{
    mDaysOfWeek = days;
}

/**
 * @brief setDays
 * @param days
 */
void ChannelData::setDays( const QString &days)
{
    mDays = days;
}

/**
 * @brief setMonths
 * @param months
 */
void ChannelData::setMonths( const QString &months)
{
    mMonths = months;
}

/**
 * @brief setVolume         - Установка громкости
 * @param volume    - значение громкости 0-100
 */
void ChannelData::setVolume(int volume)
{
    mVolume = volume;
}

/**
 * @brief ChannelData::getChannelName
 * @return
 */
QString ChannelData::channelName() const
{
    return mChannelName;
}


int ChannelData::getNum() const
{
    return mId;
}


QTime ChannelData::startTime() const
{
    return mStartTime;
}


QTime ChannelData::endTime() const
{
    return mEndTime;
}


QString ChannelData::daysOfWeek() const
{
    return mDaysOfWeek;
}


QString ChannelData::days() const
{
    return mDays;
}


QString ChannelData::months() const
{
    return mMonths;
}


int ChannelData::volume() const
{
    return mVolume;
}

}
