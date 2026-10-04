
#pragma once
#ifndef CHANNELDATA_H
#define CHANNELDATA_H

#include "mediamanager.h"
#include "mediaboxmanagerdata.h"

#include <QString>
#include <QStringList>
#include <QDateTime>

namespace MediaBoxManager {

class ChannelData
{
public:
    ChannelData(CHANNEL_TYPE type);
    ChannelData();
    ~ChannelData();

    /**
     * @brief setNum
     * @param id
     */
    void setNum( int id );

    /**
     * @brief setChannelName
     * @param name
     */
    void setChannelName(const QString &name );
    void setRuleId(const QString &id) { mRuleId = id; }
    QString ruleId() const { return mRuleId; }
    void setPlaybackOrder(const QString &order) { mPlaybackOrder = order; }
    QString playbackOrder() const { return mPlaybackOrder; }

    /**
     * @brief setStartTime
     * @param time
     */
    void setStartTime( const QTime &time );

    /**
     * @brief setEndTime
     * @param time
     */
    void setEndTime( const QTime &time );

    /**
     * @brief setDaysOfWeek
     * @param days
     */
    void setDaysOfWeek( const QString &days);

    /**
     * @brief setDays
     * @param days
     */
    void setDays( const QString &days);

    /**
     * @brief setMonths
     * @param months
     */
    void setMonths( const QString &months);

    /**
     * @brief setVolume         - Установка громкости
     * @param volume    - значение громкости 0-100
     */
    void setVolume(int volume);

    /**
     * @brief getNum
     * @return
     */
    int getNum() const;

    /**
     * @brief getChannelName
     * @return
     */
    QString channelName() const;

    /**
     * @brief getStartTime
     * @return
     */
    QTime startTime() const;

    /**
     * @brief endTime
     * @return
     */
    QTime endTime() const;


    /**
     * @brief daysOfWeek
     * @return
     */
    QString daysOfWeek() const;

    /**
     * @brief days
     * @return
     */
    QString days() const;

    /**
     * @brief months
     * @return
     */
    QString months() const;

    /**
     * @brief volume            - Возвращает громкость канала
     * @return
     */
    int volume() const;

    bool isValid();

    MediaManager& mediaManager()
    {
        return mMediaManager;
    }

private:
    /// Номер
    int                                 mId = -1;

    /// Наименование
    QString                             mChannelName;
    QString                             mRuleId;
    QString                             mPlaybackOrder = QStringLiteral("shuffle_cycle");

    /// Список музыкальных треков
    MediaManager                        mMediaManager;

    /// Время начала проигрывания
    QTime                               mStartTime;

    /// Время окончания проигрывания
    QTime                               mEndTime;

    /// Дни недели
    QString                             mDaysOfWeek;

    /// Дни
    QString                             mDays;

    /// Месяцы
    QString                             mMonths;

    /// Громкость
    int                                 mVolume = 100;

    bool                                mValid;

};

}
#endif // CHANNELDATA_H
