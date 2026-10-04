#include "schedulecompiler.h"
#include "schedulev1.h"
#include "schedulecore.h"

#include <QDir>
#include <QTimeZone>
#include <QUuid>
#include <algorithm>
#include <limits>

namespace ScheduleCompiler {
namespace {
bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString derived(const QString &space, const QString &name)
{
    return QUuid::createUuidV5(QUuid(space), name.toUtf8()).toString(QUuid::WithoutBraces);
}
QJsonArray calendar(const QString &text, int minimum, int maximum, bool weekdays = false)
{
    auto values = ScheduleCore::parseCalendar(text, minimum, maximum).values.values();
    if (weekdays) for (int &value : values) if (value == 0) value = 7;
    std::sort(values.begin(), values.end());
    QJsonArray result;
    for (int value : values) result.append(value);
    return result;
}
}

bool fromChannels(const QJsonObject &channels, const QString &contentRoot, const QJsonObject &previous,
             QJsonObject *document, QString *error, const QString &mediaType)
{
    if (mediaType != QStringLiteral("audio") && mediaType != QStringLiteral("video"))
        return fail(error, QStringLiteral("Неизвестный тип контента расписания."));
    if (!document || !channels.value("channels").isArray() || !channels.value("adverts").isArray())
        return fail(error, QStringLiteral("Не заданы каналы и рекламные правила для компиляции."));
    const QString scheduleId = previous.value("scheduleId").toString(uuid());
    const QString stationId = previous.value("stationId").toString(uuid());
    const QDate today = QDate::currentDate();
    auto validity = previous.value("validity").toObject();
    // Channel editing represents recurring calendar conditions. Renew its generated
    // horizon ahead of expiry; explicitly edited projects retain their range.
    const QDate oldUntil = QDate::fromString(validity.value("until").toString(), Qt::ISODate);
    if (!oldUntil.isValid() || oldUntil <= today.addDays(30))
        validity = {{"from", today.toString(Qt::ISODate)}, {"until", today.addYears(1).toString(Qt::ISODate)}};
    QJsonObject result{{"$schema", "urn:mediabox:schedule:v1"}, {"format", "mediabox.schedule"},
        {"schemaVersion", 1}, {"scheduleId", scheduleId}, {"stationId", stationId},
        {"publicationId", previous.value("publicationId").toString(uuid())},
        {"revision", qMax(1, previous.value("revision").toInt())},
        {"publishedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"timeZone", previous.value("timeZone").toString(QString::fromUtf8(QTimeZone::systemTimeZoneId()))},
        {"validity", validity},
        {"musicTransition", "finish_track"}, {"timeResolution", QJsonObject{{"gap", "skip"}, {"overlap", "first"}}},
        {"fallback", QJsonObject{{"source", QJsonObject{{"type", "silence"}}}, {"volumePercent", 0}}},
        {"calendars", QJsonArray{}}, {"mixRules", QJsonArray{}}};
    QJsonArray assets, playlists, templates, rules, events;
    QSet<QString> assetIds;
    const QDir root(contentRoot);
    const auto asset = [&](const QString &path, const QString &identity) -> QString {
        const QString relative = QDir::fromNativeSeparators(root.relativeFilePath(path));
        if (QDir::isAbsolutePath(relative) || relative.startsWith("../") || relative.contains("/../")) {
            fail(error, QStringLiteral("Медиафайл находится вне каталога контента: %1").arg(path));
            return {};
        }
        const QString id = derived(scheduleId, "asset:" + identity);
        if (!assetIds.contains(id)) {
            assets.append(QJsonObject{{"id", id}, {"path", relative}, {"mediaType", mediaType}});
            assetIds.insert(id);
        }
        return id;
    };
    QHash<QString, QJsonObject> priorPlaylists;
    for (const auto &item : previous.value("playlists").toArray())
        priorPlaylists.insert(item.toObject().value("id").toString(), item.toObject());
    for (const auto &value : channels.value("channels").toArray()) {
        const auto channel = value.toObject();
        const QString id = channel.value("id").toString(), name = channel.value("name").toString();
        const QTime start = QTime::fromString(channel.value("start").toString(), "HH:mm");
        const QTime end = QTime::fromString(channel.value("end").toString(), "HH:mm");
        const int offset = channel.value("untilDayOffset").toInt();
        if (!start.isValid() || !end.isValid() || (offset == 0 && start >= end)
                || (offset == 1 && end > start))
            return fail(error, QStringLiteral("Канал «%1»: задайте непустой интервал, «Полные сутки» или перенос окончания.").arg(name));
        QJsonArray entries;
        QHash<QString, int> occurrences;
        for (const auto &path : channel.value("paths").toArray()) {
            QString identity = QDir::fromNativeSeparators(root.relativeFilePath(path.toString()));
            const QString channelPrefix = (mediaType == "video" ? "video/" : "music/") + channel.value("directory").toString(name) + "/";
            if (identity.startsWith(channelPrefix)) identity.remove(0, channelPrefix.size());
            const QString assetId = asset(path.toString(), id + ":" + identity);
            if (assetId.isEmpty()) return false;
            entries.append(QJsonObject{{"id", derived(id, "entry:" + assetId + ":" + QString::number(occurrences[assetId]++))}, {"assetId", assetId}});
        }
        const auto prior = priorPlaylists.value(id);
        int revision = qMax(1, prior.value("revision").toInt());
        const QString order = channel.value("order").toString("shuffle_cycle");
        if (!prior.isEmpty() && (prior.value("entries") != entries || prior.value("order") != order)) {
            if (revision == std::numeric_limits<int>::max()) return fail(error, QStringLiteral("Исчерпана ревизия плейлиста."));
            ++revision;
        }
        playlists.append(QJsonObject{{"id", id}, {"revision", revision}, {"name", name}, {"order", order}, {"entries", entries}});
        const QString templateId = derived(id, "template");
        const QJsonObject window{{"from", start.toString("HH:mm:ss")}, {"until", end.toString("HH:mm:ss")}, {"untilDayOffset", offset}};
        templates.append(QJsonObject{{"id", templateId}, {"name", name}, {"slots", QJsonArray{QJsonObject{
            {"id", derived(id, "slot")}, {"window", window}, {"source", QJsonObject{{"type", "playlist"}, {"playlistId", id}}},
            {"volumePercent", channel.value("volume")}}}}});
        const QJsonObject select{{"type", "annual_filter"},
            {"months", calendar(channel.value("months").toString(), 1, 12)},
            {"monthDays", calendar(channel.value("days").toString(), 1, 31)},
            {"weekdays", calendar(channel.value("weekdays").toString(), 0, 6, true)}};
        rules.append(QJsonObject{{"id", derived(id, "base")}, {"name", name}, {"enabled", true}, {"priority", 0},
            {"when", QJsonObject{{"select", select}, {"excludeDates", QJsonArray{}}}}, {"templateId", templateId}});
    }
    for (const auto &value : channels.value("adverts").toArray()) {
        const auto advert = value.toObject();
        const auto startModeValue = advert.value("startMode");
        const QString startMode = startModeValue.isUndefined() ? QStringLiteral("interrupt") : startModeValue.toString();
        if ((!startModeValue.isUndefined() && !startModeValue.isString())
                || (startMode != QStringLiteral("interrupt") && startMode != QStringLiteral("after_track")))
            return fail(error, QStringLiteral("Неизвестный способ начала рекламы."));
        if (advert.value("timing") == QJsonValue("*")) continue;
        const QString id = advert.value("id").toString(), name = advert.value("name").toString();
        const QJsonArray paths = advert.value("paths").toArray();
        // Missing content remains a valid, diagnosed event rather than silently
        // deleting its schedule. Its path is known from the advert filename.
        const QString path = paths.isEmpty() ? root.filePath("ads/" + name) : paths.first().toString();
        const QString assetId = asset(path, id);
        if (assetId.isEmpty()) return false;
        ScheduleCore::AdvertRule sourceRule;
        sourceRule.stableId = id;
        sourceRule.name = name;
        sourceRule.hours = advert.value("hours").toString();
        sourceRule.weekdays = advert.value("weekdays").toString();
        sourceRule.from = QDate::fromString(advert.value("from").toString(), Qt::ISODate);
        sourceRule.until = QDate::fromString(advert.value("until").toString(), Qt::ISODate);
        sourceRule.timing = advert.value("timing").toString();
        sourceRule.volume = advert.value("volume").toInt();
        sourceRule.startMode = startMode;
        for (const auto &minute : advert.value("compiledMinutes").toArray())
            sourceRule.compiledMinutes.append(minute.toInt());
        const QString timingError = ScheduleCore::validateAdvert(sourceRule);
        if (!timingError.isEmpty()) return fail(error, name + ": " + timingError);
        const auto minutes = ScheduleCore::compileAdvertMinutes(sourceRule);
        QJsonArray times;
        for (const auto &hour : calendar(advert.value("hours").toString(), 0, 23))
            for (int minute : minutes)
                times.append(QTime(hour.toInt(), minute).toString("HH:mm:ss"));
        const QDate from = QDate::fromString(advert.value("from").toString(), Qt::ISODate);
        const QDate until = QDate::fromString(advert.value("until").toString(), Qt::ISODate).addDays(1);
        events.append(QJsonObject{{"id", id}, {"name", name}, {"enabled", true}, {"priority", 0},
            {"when", QJsonObject{{"select", QJsonObject{{"type", "weekdays"},
                {"days", calendar(advert.value("weekdays").toString(), 0, 6, true)}}},
                {"range", QJsonObject{{"from", from.toString(Qt::ISODate)}, {"until", until.toString(Qt::ISODate)}}},
                {"excludeDates", QJsonArray{}}}}, {"times", times},
            {"action", QJsonObject{{"assetId", assetId}, {"volumePercent", advert.value("volume")}}},
            {"delivery", QJsonObject{{"start", startMode}, {"maxLateSeconds", startMode == "after_track" ? 3600 : 59},
                {"expired", "skip"}, {"after", "resume_music"}}}});
    }
    result.insert("assets", assets); result.insert("playlists", playlists); result.insert("dayTemplates", templates);
    result.insert("baseRules", rules); result.insert("eventRules", events);
    result.insert("requiredCapabilities", QJsonArray::fromStringList(ScheduleV1::requiredCapabilities(result)));
    *document = result;
    return true;
}
}
