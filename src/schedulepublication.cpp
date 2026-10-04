#include "schedulepublication.h"
#include "schedulecore/schedulev1.h"
#include "schedulecore/schedulecore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QTimeZone>
#include <QUuid>
#include <algorithm>
#include <limits>

namespace SchedulePublication {
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
QByteArray json(const QJsonObject &object) { return QJsonDocument(object).toJson(QJsonDocument::Indented); }
bool read(const QString &path, QByteArray *bytes, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, path + ": " + file.errorString());
    *bytes = file.readAll();
    return file.error() == QFileDevice::NoError || fail(error, path + ": " + file.errorString());
}
bool write(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail(error, path + ": " + file.errorString());
    return true;
}
bool loadProject(const QString &directory, QJsonObject *project, QString *error)
{
    if (!QFileInfo::exists(projectPath(directory))) {
        *project = {{"format", "mediabox.schedule-project"}, {"schemaVersion", 1},
                    {"mode", "channels"}, {"lastRevision", 0}};
        return true;
    }
    QByteArray bytes;
    if (!read(projectPath(directory), &bytes, error)) return false;
    const QString reason = ScheduleV1::strictJsonObject(bytes, project);
    if (!reason.isEmpty()) return fail(error, reason);
    const auto mode = project->value("mode").toString();
    if (project->value("format") != QJsonValue("mediabox.schedule-project")
            || project->value("schemaVersion") != QJsonValue(1)
            || (mode != "channels" && mode != "advanced") || !project->value("document").isObject()
            || !project->value("lastRevision").isDouble()
            || project->value("lastRevision").toDouble() != project->value("lastRevision").toInt(-1)
            || project->value("lastRevision").toInt(-1) < 0)
        return fail(error, QStringLiteral("Некорректный проект расписания: %1").arg(projectPath(directory)));
    return true;
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
QJsonObject comparable(QJsonObject document)
{
    document.remove("publicationId"); document.remove("revision"); document.remove("publishedAt");
    return document;
}
bool compile(const QJsonObject &legacy, const QString &contentRoot, const QJsonObject &previous,
             QJsonObject *document, QString *error)
{
    const QString scheduleId = previous.value("scheduleId").toString(uuid());
    const QString stationId = previous.value("stationId").toString(uuid());
    const QDate today = QDate::currentDate();
    auto validity = previous.value("validity").toObject();
    // Channel mode represents recurring legacy conditions. Renew its generated
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
            assets.append(QJsonObject{{"id", id}, {"path", relative}, {"mediaType", "audio"}});
            assetIds.insert(id);
        }
        return id;
    };
    QHash<QString, QJsonObject> priorPlaylists;
    for (const auto &item : previous.value("playlists").toArray())
        priorPlaylists.insert(item.toObject().value("id").toString(), item.toObject());
    for (const auto &value : legacy.value("channels").toArray()) {
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
            const QString channelPrefix = "music/" + name + "/";
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
    for (const auto &value : legacy.value("adverts").toArray()) {
        const auto advert = value.toObject();
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
            {"delivery", QJsonObject{{"start", "interrupt"}, {"maxLateSeconds", 59}, {"expired", "skip"}, {"after", "resume_music"}}}});
    }
    result.insert("assets", assets); result.insert("playlists", playlists); result.insert("dayTemplates", templates);
    result.insert("baseRules", rules); result.insert("eventRules", events);
    result.insert("requiredCapabilities", QJsonArray::fromStringList(ScheduleV1::requiredCapabilities(result)));
    *document = result;
    return true;
}
}

QString projectPath(const QString &directory) { return QDir(directory).filePath("schedule-project.json"); }

bool draft(const QString &directory, const QString &contentRoot, const QJsonObject &legacy,
           QJsonObject *document, bool *advanced, QString *error)
{
    if (error) error->clear();
    if (!QDir().mkpath(directory)) return fail(error, QStringLiteral("Не удалось создать каталог проекта расписания."));
    QLockFile lock(projectPath(directory) + ".lock");
    if (!lock.tryLock(3000)) return fail(error, QStringLiteral("Проект расписания занят другим редактором."));
    QJsonObject project;
    if (!loadProject(directory, &project, error)) return false;
    const bool saved = project.value("mode") == QJsonValue("advanced");
    if (advanced) *advanced = saved;
    if (saved) { *document = project.value("document").toObject(); return true; }
    QJsonObject result;
    if (!compile(legacy, contentRoot, project.value("document").toObject(), &result, error)) return false;
    project.insert("document", result);
    if (!write(projectPath(directory), json(project), error)) return false;
    *document = result;
    return true;
}

bool saveDraft(const QString &directory, const QJsonObject &document, QString *error)
{
    ScheduleV1::Document verified;
    const QString reason = ScheduleV1::decode(document, &verified);
    if (!reason.isEmpty()) return fail(error, reason);
    QLockFile lock(projectPath(directory) + ".lock");
    if (!lock.tryLock(3000)) return fail(error, QStringLiteral("Проект расписания занят другим редактором."));
    QJsonObject project;
    if (!loadProject(directory, &project, error)) return false;
    const auto existing = project.value("document").toObject();
    auto saved = document;
    // Import copies rules into this station's project; it does not silently
    // retarget the station or reset the monotonically increasing publication.
    if (!existing.isEmpty()) {
        saved.insert("scheduleId", existing.value("scheduleId"));
        saved.insert("stationId", existing.value("stationId"));
    }
    const QString savedError = ScheduleV1::decode(saved, &verified);
    if (!savedError.isEmpty()) return fail(error, savedError);
    project.insert("document", saved); project.insert("mode", "advanced");
    return write(projectPath(directory), json(project), error);
}

bool publish(const QString &directory, const QString &contentRoot, const QJsonObject &document,
             Publication *publication, QString *error)
{
    if (error) error->clear();
    QLockFile lock(projectPath(directory) + ".lock");
    if (!lock.tryLock(3000)) return fail(error, QStringLiteral("Проект расписания занят другим редактором."));
    QJsonObject project;
    if (!loadProject(directory, &project, error)) return false;
    const auto expected = project.value("document").toObject();
    if (!expected.isEmpty() && comparable(expected) != comparable(document))
        return fail(error, QStringLiteral("Проект изменён другим редактором. Откройте его заново перед публикацией."));
    const QDir dir(directory);
    const QString activePath = dir.filePath("active.json");
    int previousRevision = project.value("lastRevision").toInt();
    if (QFileInfo::exists(activePath)) {
        QByteArray activeBytes, previousBytes;
        QJsonObject active;
        if (!read(activePath, &activeBytes, error)) return false;
        const auto reason = ScheduleV1::strictJsonObject(activeBytes, &active);
        if (!reason.isEmpty()) return fail(error, reason);
        const QString relative = QStringLiteral("snapshots/%1.json").arg(active.value("publicationId").toString());
        if (QUuid(active.value("publicationId").toString()).isNull()
                || QUuid(active.value("publicationId").toString()).toString(QUuid::WithoutBraces) != active.value("publicationId").toString()
                || active.value("snapshotPath").toString() != relative)
            return fail(error, QStringLiteral("Некорректный путь опубликованного снимка."));
        if (!read(dir.filePath(relative), &previousBytes, error)) return false;
        if (QString::fromLatin1(QCryptographicHash::hash(previousBytes, QCryptographicHash::Sha256).toHex())
                != active.value("sha256").toString()) return fail(error, QStringLiteral("Контрольная сумма предыдущего снимка не совпадает."));
        ScheduleV1::Document previous;
        const QString priorError = ScheduleV1::parse(previousBytes, &previous);
        if (!priorError.isEmpty()) return fail(error, priorError);
        if (active.size() != 8 || active.value("format") != QJsonValue("mediabox.active")
                || active.value("schemaVersion") != QJsonValue(1)
                || active.value("scheduleId") != previous.object.value("scheduleId")
                || active.value("stationId") != previous.object.value("stationId")
                || active.value("publicationId") != previous.object.value("publicationId")
                || active.value("revision") != previous.object.value("revision"))
            return fail(error, QStringLiteral("Указатель выпуска не соответствует снимку расписания."));
        previousRevision = qMax(previousRevision, previous.revision());
        if (comparable(previous.object) == comparable(document)) {
            *publication = {active, previousBytes, activePath, contentRoot};
            return true;
        }
    }
    if (previousRevision == std::numeric_limits<int>::max()) return fail(error, QStringLiteral("Исчерпан номер выпуска."));
    QJsonObject released = document;
    released.insert("publicationId", uuid()); released.insert("revision", previousRevision + 1);
    released.insert("publishedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    released.insert("requiredCapabilities", QJsonArray::fromStringList(ScheduleV1::requiredCapabilities(released)));
    ScheduleV1::Document verified;
    const QString reason = ScheduleV1::decode(released, &verified);
    if (!reason.isEmpty()) return fail(error, reason);
    const QByteArray bytes = json(released);
    const QString digest = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    const QString id = released.value("publicationId").toString(), relative = "snapshots/" + id + ".json";
    const QJsonObject active{{"format", "mediabox.active"}, {"schemaVersion", 1},
        {"scheduleId", released.value("scheduleId")}, {"stationId", released.value("stationId")},
        {"publicationId", id}, {"revision", previousRevision + 1}, {"snapshotPath", relative}, {"sha256", digest}};
    if (!dir.mkpath("publications") || !dir.mkpath("snapshots"))
        return fail(error, QStringLiteral("Не удалось создать каталог выпусков расписания."));
    // Persist the exact release before exposing its pointer. Failed publication
    // can leave a recoverable inactive release, never a partially written one.
    project.insert("document", released); project.insert("lastRevision", previousRevision + 1);
    if (!write(projectPath(directory), json(project), error)
            || !write(dir.filePath("publications/" + id + ".json"),
                json({{"active", active}, {"snapshotBase64", QString::fromLatin1(bytes.toBase64())}}), error)) return false;
    if (QFileInfo::exists(dir.filePath(relative))) return fail(error, QStringLiteral("Снимок с таким ID уже существует."));
    if (!write(dir.filePath(relative), bytes, error)) return false;
    QByteArray readback;
    if (!read(dir.filePath(relative), &readback, error) || readback != bytes)
        return fail(error, QStringLiteral("Не удалось подтвердить байты записанного снимка."));
    if (!write(activePath, json(active), error)) return false;
    *publication = {active, bytes, activePath, contentRoot};
    return true;
}
}
