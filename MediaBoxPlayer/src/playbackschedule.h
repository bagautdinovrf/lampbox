#pragma once

#include "schedulecore/schedulecore.h"

#include <QJsonObject>
#include <QStringList>

namespace MediaBox {

QString playbackFileError(const QString &path);

struct ScheduledChannel {
    ScheduleCore::ChannelRule rule;
    QStringList paths;
    QString order = QStringLiteral("shuffle_cycle");
};

struct ScheduledAdvert {
    ScheduleCore::AdvertRule rule;
    QStringList paths;
};

// An immutable, validated snapshot received from the manager. All temporal
// interpretation remains in ScheduleCore, shared with the manager's preview.
struct PlaybackSchedule {
    QList<ScheduledChannel> channels;
    QList<ScheduledAdvert> adverts;

    QList<ScheduleCore::ChannelRule> channelRules() const;
    QList<ScheduleCore::AdvertRule> advertRules() const;
    static QString decode(const QJsonObject &object, PlaybackSchedule *result);
};

} // namespace MediaBox
