#include "schedulepublication.h"
#include "schedulecore/schedulev1.h"
#include "schedulecore/schedulecompiler.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHash>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
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
            || project->value("lastRevision").toInt(-1) < 0
            || (project->contains("channelDocument") && !project->value("channelDocument").isObject()))
        return fail(error, QStringLiteral("Некорректный проект расписания: %1").arg(projectPath(directory)));
    return true;
}
QJsonObject comparable(QJsonObject document)
{
    document.remove("publicationId"); document.remove("revision"); document.remove("publishedAt");
    return document;
}

QString derived(const QString &space, const QString &name)
{
    return QUuid::createUuidV5(QUuid(space), name.toUtf8()).toString(QUuid::WithoutBraces);
}
QHash<QString, QJsonObject> byId(const QJsonArray &array)
{
    QHash<QString, QJsonObject> result;
    for (const auto &value : array) {
        const auto object = value.toObject();
        result.insert(object.value("id").toString(), object);
    }
    return result;
}
bool idArray(const QJsonArray &array)
{
    QSet<QString> seen;
    for (const auto &value : array) {
        const QString id = value.toObject().value("id").toString();
        if (id.isEmpty() || seen.contains(id)) return false;
        seen.insert(id);
    }
    return true;
}

// Only edits made in the channel editor win over a manually edited project.
// Object members and stable-ID array entries untouched by that edit survive.
QJsonValue channelDelta(const QJsonValue &before, const QJsonValue &after, const QJsonValue &current)
{
    if (before == after) return current;
    // A manually removed entity stays removed when its former channel changes;
    // additions have no baseline object and still enter the project normally.
    if (before.isObject() && after.isObject() && current.isUndefined()) return current;
    if (before.isObject() && after.isObject() && current.isObject()) {
        auto result = current.toObject();
        const auto oldObject = before.toObject(), newObject = after.toObject();
        // A changed channel calendar replaces a different selector kind as a
        // unit; mixing calendarId with annual_filter fields is not valid JSON
        // under the schedule contract.
        if (newObject.contains("type") && result.value("type") != newObject.value("type")) return after;
        QSet<QString> keys;
        for (auto it = oldObject.begin(); it != oldObject.end(); ++it) keys.insert(it.key());
        for (auto it = newObject.begin(); it != newObject.end(); ++it) keys.insert(it.key());
        for (const auto &key : keys) {
            const auto value = channelDelta(oldObject.value(key), newObject.value(key), result.value(key));
            if (value.isUndefined()) result.remove(key);
            else result.insert(key, value);
        }
        return result;
    }
    if (before.isArray() && after.isArray() && current.isArray()
            && idArray(before.toArray()) && idArray(after.toArray()) && idArray(current.toArray())) {
        const auto oldArray = before.toArray(), newArray = after.toArray(), currentArray = current.toArray();
        const auto oldItems = byId(oldArray), newItems = byId(newArray), currentItems = byId(currentArray);
        QStringList oldOrder, newOrder, order;
        for (const auto &value : oldArray) oldOrder << value.toObject().value("id").toString();
        for (const auto &value : newArray) newOrder << value.toObject().value("id").toString();
        for (const auto &value : currentArray) {
            const QString id = value.toObject().value("id").toString();
            if (!oldItems.contains(id) || newItems.contains(id)) order << id;
        }
        QStringList managedOrder, previousManagedOrder;
        for (const auto &id : newOrder)
            if (oldItems.contains(id) && currentItems.contains(id)) managedOrder << id;
        for (const auto &id : oldOrder)
            if (newItems.contains(id) && currentItems.contains(id)) previousManagedOrder << id;
        if (previousManagedOrder != managedOrder) {
            int managed = 0;
            for (auto &id : order)
                if (oldItems.contains(id) && newItems.contains(id)) id = managedOrder.at(managed++);
        }
        for (qsizetype i = 0; i < newOrder.size(); ++i) {
            const QString id = newOrder[i];
            if (oldItems.contains(id) || order.contains(id)) continue;
            qsizetype position = order.size();
            for (qsizetype next = i + 1; next < newOrder.size(); ++next) {
                const auto neighbor = order.indexOf(newOrder[next]);
                if (neighbor >= 0) { position = neighbor; break; }
            }
            order.insert(position, id);
        }
        QJsonArray result;
        for (const auto &id : order) {
            const QJsonValue oldValue = oldItems.contains(id) ? QJsonValue(oldItems.value(id)) : QJsonValue(QJsonValue::Undefined);
            const QJsonValue newValue = newItems.contains(id) ? QJsonValue(newItems.value(id)) : QJsonValue(QJsonValue::Undefined);
            const QJsonValue currentValue = currentItems.contains(id) ? QJsonValue(currentItems.value(id)) : QJsonValue(QJsonValue::Undefined);
            const auto value = channelDelta(oldValue, newValue, currentValue);
            if (!value.isUndefined()) result.append(value);
        }
        return result;
    }
    return after;
}

// Older envelopes have no baseline. Recognize generated UUIDv5 relationships
// rather than interpreting arbitrary advanced entities as channel-owned data.
QJsonObject legacyChannelDocument(const QJsonObject &saved, const QJsonObject &next)
{
    auto baseline = next;
    const auto savedTemplates = byId(saved.value("dayTemplates").toArray());
    const auto savedRules = byId(saved.value("baseRules").toArray());
    const auto nextTemplates = byId(next.value("dayTemplates").toArray());
    const auto nextRules = byId(next.value("baseRules").toArray());
    const auto savedAssets = byId(saved.value("assets").toArray());
    QJsonArray playlists, templates, rules, events, assets;
    QSet<QString> assetIds;
    const auto addAsset = [&](const QString &id) {
        if (!assetIds.contains(id) && savedAssets.contains(id)) {
            assets.append(savedAssets.value(id)); assetIds.insert(id);
        }
    };
    for (const auto &value : saved.value("playlists").toArray()) {
        auto playlist = value.toObject();
        const QString id = playlist.value("id").toString();
        const QString templateId = derived(id, "template"), ruleId = derived(id, "base");
        auto day = savedTemplates.value(templateId), rule = savedRules.value(ruleId);
        const auto daySlots = byId(day.value("slots").toArray());
        auto generatedSlot = daySlots.value(derived(id, "slot"));
        const bool generated = rule.value("templateId") == QJsonValue(templateId) && !generatedSlot.isEmpty();
        // An existing playlist without its original generated rule may have had
        // that rule intentionally removed. Do not recreate it during migration.
        if (!generated && !nextTemplates.contains(templateId)) continue;
        QJsonArray entries;
        QHash<QString, int> occurrences;
        for (const auto &entryValue : playlist.value("entries").toArray()) {
            const auto entry = entryValue.toObject();
            const QString assetId = entry.value("assetId").toString();
            const QString expected = derived(id, "entry:" + assetId + ":" + QString::number(occurrences[assetId]++));
            if (entry.value("id") == QJsonValue(expected)) { entries.append(entry); addAsset(assetId); }
        }
        playlist.insert("entries", entries); playlists.append(playlist);
        if (generated) {
            // Priority, exclusions and custom selector kinds belong to the
            // project; the simple channel editor cannot have created them.
            rule.insert("priority", 0); rule.insert("enabled", true);
            auto condition = rule.value("when").toObject();
            condition.remove("range"); condition.insert("excludeDates", QJsonArray{});
            if (condition.value("select").toObject().value("type") != QJsonValue("annual_filter")
                    && nextRules.contains(ruleId))
                condition.insert("select", nextRules.value(ruleId).value("when").toObject().value("select"));
            rule.insert("when", condition);
            generatedSlot.insert("source", QJsonObject{{"type", "playlist"}, {"playlistId", id}});
            day.insert("slots", QJsonArray{generatedSlot});
            templates.append(day); rules.append(rule);
        } else {
            templates.append(nextTemplates.value(templateId)); rules.append(nextRules.value(ruleId));
        }
    }
    for (const auto &value : saved.value("eventRules").toArray()) {
        auto rule = value.toObject();
        const QString id = rule.value("id").toString();
        const QString assetId = rule.value("action").toObject().value("assetId").toString();
        if (assetId != derived(saved.value("scheduleId").toString(), "asset:" + id)) continue;
        rule.insert("priority", 0); rule.insert("enabled", true);
        // The old simple editor generated only interrupt/59. Reconstruct that
        // baseline so the first explicit mode change applies during migration.
        rule.insert("delivery", QJsonObject{{"start", "interrupt"}, {"maxLateSeconds", 59}, {"expired", "skip"}, {"after", "resume_music"}});
        auto condition = rule.value("when").toObject(); condition.insert("excludeDates", QJsonArray{}); rule.insert("when", condition);
        events.append(rule); addAsset(assetId);
    }
    baseline.insert("assets", assets); baseline.insert("playlists", playlists);
    baseline.insert("dayTemplates", templates); baseline.insert("baseRules", rules); baseline.insert("eventRules", events);
    return baseline;
}

void preserveReferencedEntities(const QJsonObject &before, QJsonObject *after)
{
    const QHash<QString, QString> sections{{"assetId", "assets"}, {"playlistId", "playlists"},
        {"templateId", "dayTemplates"}, {"calendarId", "calendars"}};
    QHash<QString, QSet<QString>> references;
    const auto collect = [&](auto &&self, const QJsonValue &value) -> void {
        if (value.isArray()) {
            for (const auto &item : value.toArray()) self(self, item);
        } else if (value.isObject()) {
            const auto object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it) {
                if (sections.contains(it.key())) references[sections.value(it.key())].insert(it.value().toString());
                self(self, it.value());
            }
        }
    };
    // Channel removal must not erase content still used by a custom mix,
    // event, fallback or day template. Keep only the referenced dependency
    // closure; these entities no longer belong to the generated baseline.
    bool restored;
    do {
        restored = false; references.clear(); collect(collect, *after);
        for (auto it = references.cbegin(); it != references.cend(); ++it) {
            auto items = after->value(it.key()).toArray(); const auto remaining = byId(items);
            for (const auto &value : before.value(it.key()).toArray()) {
                const QString id = value.toObject().value("id").toString();
                if (it.value().contains(id) && !remaining.contains(id)) { items.append(value); restored = true; }
            }
            after->insert(it.key(), items);
        }
    } while (restored);
}
}

QString projectPath(const QString &directory) { return QDir(directory).filePath("schedule-project.json"); }

bool draft(const QString &directory, const QString &contentRoot, const QJsonObject &channels,
           QJsonObject *document, bool *advanced, QString *error, const QString &mediaType)
{
    if (error) error->clear();
    if (!document) return fail(error, QStringLiteral("Не задан получатель проекта расписания."));
    *document = {};
    if (advanced) *advanced = false;
    if (!QDir().mkpath(directory)) return fail(error, QStringLiteral("Не удалось создать каталог проекта расписания."));
    QLockFile lock(projectPath(directory) + ".lock");
    if (!lock.tryLock(3000)) return fail(error, QStringLiteral("Проект расписания занят другим редактором."));
    QJsonObject project;
    if (!loadProject(directory, &project, error)) return false;
    const bool saved = project.value("mode") == QJsonValue("advanced");
    if (advanced) *advanced = saved;
    const auto existing = project.value("document").toObject();
    auto baseline = project.value("channelDocument").toObject();
    QJsonObject result;
    if (!ScheduleCompiler::fromChannels(channels, contentRoot, saved && !baseline.isEmpty() ? baseline : existing,
                                       &result, error, mediaType)) return false;
    if (saved) {
        if (baseline.isEmpty()) baseline = legacyChannelDocument(existing, result);
        // A baseline is compiler state, not a new release. Keep its publication
        // metadata stable so a timer tick does not rewrite an unchanged project.
        for (const auto *key : {"publicationId", "revision", "publishedAt"})
            if (baseline.contains(key)) result.insert(key, baseline.value(key));
        const auto generated = result;
        result = existing;
        for (const auto *section : {"assets", "playlists", "dayTemplates", "baseRules", "eventRules"})
            result.insert(section, channelDelta(baseline.value(section), generated.value(section), existing.value(section)));
        preserveReferencedEntities(existing, &result);
        const auto previousLists = byId(existing.value("playlists").toArray());
        auto lists = result.value("playlists").toArray();
        for (qsizetype i = 0; i < lists.size(); ++i) {
            auto list = lists[i].toObject(); const auto previous = previousLists.value(list.value("id").toString());
            if (!previous.isEmpty()) {
                const int revision = previous.value("revision").toInt();
                if (list.value("entries") != previous.value("entries") || list.value("order") != previous.value("order")) {
                    if (revision == std::numeric_limits<int>::max()) return fail(error, QStringLiteral("Исчерпана ревизия плейлиста."));
                    list.insert("revision", qMax(revision + 1, list.value("revision").toInt()));
                } else list.insert("revision", revision);
                lists[i] = list;
            }
        }
        result.insert("playlists", lists);
        result.insert("requiredCapabilities", QJsonArray::fromStringList(ScheduleV1::requiredCapabilities(result)));
        project.insert("channelDocument", generated);
    } else project.insert("channelDocument", result);
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
                json(active), error)) return false;
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
