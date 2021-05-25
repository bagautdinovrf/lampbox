#ifndef ADVERTDATA_H
#define ADVERTDATA_H


#include <QList>
#include <QString>
#include <QDate>


class AdvertData
{
public:
    AdvertData();

    ///
    void setName( const QString &name );

    ///
    void setHours( const QString &hours );

    ///
    void setMinuts( const QString &minuts );

    ///
    void setDays( const QString &days );

    ///
    void setStartDate( QDate startDate );

    ///
    void setEndDate( QDate endDate );

    ///
    void setVolume( int volume );

    ///
    QString name() const;

    ///
    QString hours() const;

    ///
    QString minuts() const;

    ///
    QString days() const;

    ///
    QDate startDate() const;

    ///
    QDate endDate() const;

    ///
    int volume() const;

private:
    /// Название
    QString                                 mName;

    /// Часы запуска
    QString                                 mHours;

    /// Минута запуска
    QString                                 mMinuts;

    /// Дни недели
    QString                                 mDayOfWeek;

    /// Начало периода
    QDate                                   mStartDate;

    /// Конец периода
    QDate                                   mEndDate;

    /// Громкость
    int                                     mVolume;

    /// В формате cron
    QString                                 mCronString;
};

#endif // ADVERTDATA_H
