#pragma once

#include "projectrepository.h"
#include <QUuid>

namespace ProjectFixture {
inline ScheduleCore::ChannelRule channel(const QString &name, QTime start, QTime end, int volume = 70)
{
    ScheduleCore::ChannelRule result;
    result.stableId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    result.name = name;
    result.storageDirectory = name;
    result.start = start;
    result.end = end;
    result.untilDayOffset = end <= start ? 1 : 0;
    result.weekdays = result.days = result.months = QStringLiteral("*");
    result.volume = volume;
    return result;
}

inline ScheduleCore::AdvertRule advert(const QString &name, const QString &hours, const QString &timing,
                                       QDate from, QDate until, int volume,
                                       const QList<int> &minutes = {}, const QString &weekdays = QStringLiteral("*"))
{
    ScheduleCore::AdvertRule result;
    result.stableId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    result.name = name;
    result.hours = hours;
    result.timing = timing;
    result.weekdays = weekdays;
    result.from = from;
    result.until = until;
    result.volume = volume;
    result.compiledMinutes = minutes;
    return result;
}
}
