#include "playbackschedule.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <utility>

namespace MediaBox {
namespace {

bool fields(const QJsonObject &object, std::initializer_list<const char *> required,
            std::initializer_list<const char *> optional = {})
{
    QSet<QString> allowed;
    for (const char *key : required) {
        if (!object.contains(QLatin1String(key)))
            return false;
        allowed.insert(QLatin1String(key));
    }
    for (const char *key : optional)
        allowed.insert(QLatin1String(key));
    for (auto it = object.begin(); it != object.end(); ++it)
        if (!allowed.contains(it.key()))
            return false;
    return true;
}

bool integer(const QJsonValue &value, int minimum, int maximum, int *result)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number || number < minimum || number > maximum)
        return false;
    *result = int(number);
    return true;
}

bool identity(const QJsonObject &object, QString *id, QString *name, QSet<QString> *ids)
{
    if (!object.value("id").isString() || !object.value("name").isString())
        return false;
    *id = object.value("id").toString();
    *name = object.value("name").toString();
    for (const QString &text : {*id, *name})
        if (text.trimmed().isEmpty() || text.size() > 256 || text.contains(QChar::Null))
            return false;
    if (ids->contains(*id))
        return false;
    ids->insert(*id);
    return true;
}

QString paths(const QJsonValue &value, QStringList *result)
{
    if (!value.isArray() || value.toArray().size() > 1000)
        return QStringLiteral("paths must be an array of at most 1000 absolute local file paths.");
    for (const auto &entry : value.toArray()) {
        if (!entry.isString())
            return QStringLiteral("Every entry in paths must be a string.");
        const QString path = entry.toString();
        const QString error = playbackFileError(path);
        if (!error.isEmpty())
            return error;
        result->append(QDir::cleanPath(path));
    }
    return {};
}

} // namespace

QString playbackFileError(const QString &path)
{
    if (path.isEmpty() || path.size() > 4096 || path.contains(QChar::Null)
        || !QDir::isAbsolutePath(path))
        return QStringLiteral("Each path must be an absolute local path of at most 4096 characters.");
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || !info.isReadable())
        return QStringLiteral("Media file does not exist or is not a readable regular file: %1").arg(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("Cannot read media file: %1").arg(path);
    return {};
}

QList<ScheduleCore::ChannelRule> PlaybackSchedule::channelRules() const
{
    QList<ScheduleCore::ChannelRule> rules;
    for (const auto &channel : channels)
        rules.append(channel.rule);
    return rules;
}

QList<ScheduleCore::AdvertRule> PlaybackSchedule::advertRules() const
{
    QList<ScheduleCore::AdvertRule> rules;
    for (const auto &advert : adverts)
        rules.append(advert.rule);
    return rules;
}

QString PlaybackSchedule::decode(const QJsonObject &object, PlaybackSchedule *result)
{
    if (!fields(object, {"channels", "adverts"}) || !object.value("channels").isArray()
        || !object.value("adverts").isArray() || object.value("channels").toArray().size() > 1000
        || object.value("adverts").toArray().size() > 1000)
        return QStringLiteral("schedule must contain channels and adverts arrays of at most 1000 rules.");
    PlaybackSchedule replacement;
    QSet<QString> ids;
    for (const auto &value : object.value("channels").toArray()) {
        if (!value.isObject())
            return QStringLiteral("Every scheduled channel must be an object.");
        const auto item = value.toObject();
        ScheduledChannel channel;
        auto &rule = channel.rule;
        if (!fields(item, {"id", "name", "start", "end", "weekdays", "days", "months", "volume", "paths"}, {"order", "untilDayOffset"})
            || !identity(item, &rule.stableId, &rule.name, &ids)
            || !item.value("start").isString() || !item.value("end").isString()
            || !item.value("weekdays").isString() || !item.value("days").isString()
            || !item.value("months").isString() || !integer(item.value("volume"), 0, 100, &rule.volume))
            return QStringLiteral("Invalid fields in scheduled channel.");
        if (item.contains("order")) {
            channel.order = item.value("order").toString();
            if (channel.order != QStringLiteral("sequential") && channel.order != QStringLiteral("shuffle_cycle"))
                return QStringLiteral("Channel order must be sequential or shuffle_cycle.");
        }
        rule.order = channel.order;
        if (item.contains("untilDayOffset") && !integer(item.value("untilDayOffset"), 0, 1, &rule.untilDayOffset))
            return QStringLiteral("Channel untilDayOffset must be 0 or 1.");
        const auto start = item.value("start").toString(), end = item.value("end").toString();
        rule.start = QTime::fromString(start, QStringLiteral("HH:mm"));
        rule.end = QTime::fromString(end, QStringLiteral("HH:mm"));
        if (!rule.start.isValid() || !rule.end.isValid() || rule.start.toString("HH:mm") != start
            || rule.end.toString("HH:mm") != end)
            return QStringLiteral("Channel start and end must use HH:mm.");
        rule.weekdays = item.value("weekdays").toString();
        rule.days = item.value("days").toString();
        rule.months = item.value("months").toString();
        const QString validation = ScheduleCore::validateChannel(rule);
        if (!validation.isEmpty())
            return validation;
        const QString pathError = paths(item.value("paths"), &channel.paths);
        if (!pathError.isEmpty())
            return pathError;
        replacement.channels.append(channel);
    }
    for (const auto &value : object.value("adverts").toArray()) {
        if (!value.isObject())
            return QStringLiteral("Every scheduled advert must be an object.");
        const auto item = value.toObject();
        ScheduledAdvert advert;
        auto &rule = advert.rule;
        if (!fields(item, {"id", "name", "hours", "weekdays", "from", "until", "timing", "volume", "paths"}, {"compiledMinutes"})
            || !identity(item, &rule.stableId, &rule.name, &ids)
            || !item.value("hours").isString() || !item.value("weekdays").isString()
            || !item.value("from").isString() || !item.value("until").isString()
            || !item.value("timing").isString() || !integer(item.value("volume"), 0, 100, &rule.volume))
            return QStringLiteral("Invalid fields in scheduled advert.");
        rule.hours = item.value("hours").toString();
        rule.weekdays = item.value("weekdays").toString();
        rule.timing = item.value("timing").toString();
        const auto from = item.value("from").toString(), until = item.value("until").toString();
        rule.from = QDate::fromString(from, Qt::ISODate);
        rule.until = QDate::fromString(until, Qt::ISODate);
        if (!rule.from.isValid() || !rule.until.isValid() || rule.from.toString(Qt::ISODate) != from
            || rule.until.toString(Qt::ISODate) != until)
            return QStringLiteral("Advert from and until must use YYYY-MM-DD.");
        if (item.contains("compiledMinutes")) {
            if (!item.value("compiledMinutes").isArray())
                return QStringLiteral("compiledMinutes must be an array of unique minutes from 0 to 59.");
            QSet<int> unique;
            for (const auto &entry : item.value("compiledMinutes").toArray()) {
                int minute = 0;
                if (!integer(entry, 0, 59, &minute) || unique.contains(minute))
                    return QStringLiteral("compiledMinutes must be an array of unique minutes from 0 to 59.");
                unique.insert(minute);
                rule.compiledMinutes.append(minute);
            }
            std::sort(rule.compiledMinutes.begin(), rule.compiledMinutes.end());
        }
        const QString validation = ScheduleCore::validateAdvert(rule);
        if (!validation.isEmpty())
            return validation;
        const QString pathError = paths(item.value("paths"), &advert.paths);
        if (!pathError.isEmpty())
            return pathError;
        replacement.adverts.append(advert);
    }
    *result = std::move(replacement);
    return {};
}

} // namespace MediaBox
