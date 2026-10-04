#include "projectrepository.h"
#include "scheduleimport.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace ProjectRepository {
namespace {
bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}
bool readBytes(const QString &path, QByteArray *bytes, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, QStringLiteral("Не удалось прочитать %1: %2").arg(path, file.errorString()));
    *bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) return fail(error, QStringLiteral("Ошибка чтения %1: %2").arg(path, file.errorString()));
    return true;
}
bool writeAtomic(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail(error, QStringLiteral("Не удалось сохранить %1: %2").arg(path, file.errorString()));
    return true;
}
bool keys(const QJsonObject &object, std::initializer_list<const char *> expected)
{
    if (object.size() != qsizetype(expected.size())) return false;
    for (const char *key : expected) if (!object.contains(QLatin1String(key))) return false;
    return true;
}
bool integer(const QJsonValue &value, int minimum, int maximum, int *result)
{
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number || number < minimum || number > maximum) return false;
    *result = int(number); return true;
}
bool integerArray(const QJsonValue &value, int minimum, int maximum, QList<int> *result, bool allowEmpty)
{
    if (!value.isArray() || (!allowEmpty && value.toArray().isEmpty())) return false;
    QSet<int> unique;
    for (const auto &item : value.toArray()) {
        int number;
        if (!integer(item, minimum, maximum, &number) || unique.contains(number)) return false;
        unique.insert(number); result->append(number);
    }
    return true;
}
bool calendar(const QJsonValue &value, int minimum, int maximum, QString *result)
{
    QList<int> numbers;
    if (!integerArray(value, minimum, maximum, &numbers, false)) return false;
    std::sort(numbers.begin(), numbers.end());
    if (numbers.size() == maximum - minimum + 1) { *result = QStringLiteral("*"); return true; }
    QStringList parts;
    for (int n : numbers) parts.append(QString::number(n));
    *result = parts.join(','); return true;
}
QJsonArray calendarJson(const QString &text, int minimum, int maximum)
{
    auto values = ScheduleCore::parseCalendar(text, minimum, maximum).values.values();
    std::sort(values.begin(), values.end());
    QJsonArray result;
    for (int n : values) result.append(n);
    return result;
}
QJsonArray minutesJson(const QList<int> &minutes)
{
    QJsonArray result;
    for (int n : minutes) result.append(n);
    return result;
}
bool identity(const QJsonObject &object, QString *id, QString *name, QSet<QString> *used)
{
    if (!object.value("id").isString() || !object.value("name").isString()) return false;
    *id = object.value("id").toString(); *name = object.value("name").toString();
    const QUuid uuid(*id);
    if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != *id || used->contains(*id) || !validFileName(*name, false)) return false;
    used->insert(*id); return true;
}
bool decodeChannels(const QJsonValue &value, QList<ScheduleCore::ChannelRule> *rules, QSet<QString> *ids,
                    bool requireDirectory)
{
    if (!value.isArray()) return false;
    QSet<QString> names;
    QSet<QString> directories;
    for (const auto &item : value.toArray()) {
        if (!item.isObject()) return false;
        auto object = item.toObject();
        ScheduleCore::ChannelRule rule;
        const bool hasDirectory = object.contains("directory");
        if (requireDirectory && !hasDirectory) return false;
        if (hasDirectory) {
            const auto directory = object.take("directory");
            if (!directory.isString() || !validFileName(directory.toString(), true)) return false;
            rule.storageDirectory = directory.toString();
        }
        // Projects saved before per-channel ordering keep the previous shuffle default.
        if (object.contains("order")) {
            if (!object.value("order").isString()) return false;
            rule.order = object.take("order").toString();
        }
        if (object.contains("untilDayOffset")
                && !integer(object.take("untilDayOffset"), 0, 1, &rule.untilDayOffset)) return false;
        if (!keys(object, {"id", "name", "start", "end", "weekdays", "days", "months", "volume"})
                || !identity(object, &rule.stableId, &rule.name, ids)
                || !object.value("start").isString() || !object.value("end").isString()
                || !calendar(object.value("weekdays"), 0, 6, &rule.weekdays)
                || !calendar(object.value("days"), 1, 31, &rule.days)
                || !calendar(object.value("months"), 1, 12, &rule.months)
                || !integer(object.value("volume"), 0, 100, &rule.volume)) return false;
        rule.start = QTime::fromString(object.value("start").toString(), "HH:mm");
        rule.end = QTime::fromString(object.value("end").toString(), "HH:mm");
        if (!hasDirectory) rule.storageDirectory = rule.name;
        if (rule.start.toString("HH:mm") != object.value("start").toString()
                || rule.end.toString("HH:mm") != object.value("end").toString()
                || names.contains(rule.name.toCaseFolded()) || directories.contains(rule.storageDirectory.toCaseFolded())
                || !ScheduleCore::validateChannel(rule).isEmpty()) return false;
        directories.insert(rule.storageDirectory.toCaseFolded());
        names.insert(rule.name.toCaseFolded()); rules->append(rule);
    }
    return true;
}
bool decodeAdverts(const QJsonValue &value, QList<ScheduleCore::AdvertRule> *rules, QSet<QString> *ids)
{
    if (!value.isArray()) return false;
    for (const auto &item : value.toArray()) {
        if (!item.isObject()) return false;
        const auto object = item.toObject();
        ScheduleCore::AdvertRule rule;
        if (!keys(object, {"id", "name", "hours", "weekdays", "from", "until", "volume", "timing", "preparedMinutes"})
                || !identity(object, &rule.stableId, &rule.name, ids)
                || !calendar(object.value("hours"), 0, 23, &rule.hours)
                || !calendar(object.value("weekdays"), 0, 6, &rule.weekdays)
                || !integer(object.value("volume"), 0, 100, &rule.volume)
                || !object.value("from").isString() || !object.value("until").isString()
                || !object.value("timing").isObject()) return false;
        rule.from = QDate::fromString(object.value("from").toString(), Qt::ISODate);
        rule.until = QDate::fromString(object.value("until").toString(), Qt::ISODate);
        if (rule.from.toString(Qt::ISODate) != object.value("from").toString()
                || rule.until.toString(Qt::ISODate) != object.value("until").toString()) return false;
        const auto timing = object.value("timing").toObject();
        if (!timing.value("kind").isString()) return false;
        const QString kind = timing.value("kind").toString();
        if (kind == "disabled" && keys(timing, {"kind"})) rule.timing = "*";
        else if (kind == "frequency" && keys(timing, {"kind", "count"})) {
            int count;
            if (!integer(timing.value("count"), 1, 5, &count)) return false;
            rule.timing = QString::number(count);
        } else if (kind == "minutes" && keys(timing, {"kind", "values"})) {
            QList<int> minutes;
            if (!integerArray(timing.value("values"), 0, 59, &minutes, false)) return false;
            std::sort(minutes.begin(), minutes.end()); rule.timing = ScheduleCore::formatMinutes(minutes);
        } else return false;
        QList<int> prepared;
        if (!integerArray(object.value("preparedMinutes"), 0, 59, &prepared, true)) return false;
        if (kind == "frequency") {
            if (prepared.isEmpty()) return false;
            rule.compiledMinutes = prepared;
        } else if (prepared != ScheduleCore::compileAdvertMinutes(rule)) return false;
        if (!ScheduleCore::validateAdvert(rule).isEmpty()) return false;
        rules->append(rule);
    }
    return true;
}
QJsonArray encodeChannels(const QList<ScheduleCore::ChannelRule> &rules)
{
    QJsonArray array;
    for (const auto &r : rules)
        array.append(QJsonObject{{"id", r.stableId}, {"name", r.name},
                     {"directory", r.storageDirectory.isEmpty() ? r.name : r.storageDirectory}, {"start", r.start.toString("HH:mm")},
                     {"end", r.end.toString("HH:mm")}, {"weekdays", calendarJson(r.weekdays, 0, 6)},
                     {"days", calendarJson(r.days, 1, 31)}, {"months", calendarJson(r.months, 1, 12)},
                     {"volume", r.volume}, {"order", r.order}, {"untilDayOffset", r.untilDayOffset}});
    return array;
}
QJsonArray encodeAdverts(const QList<ScheduleCore::AdvertRule> &rules)
{
    QJsonArray array;
    for (const auto &r : rules) {
        const auto parsed = ScheduleCore::parseAdvertTiming(r.timing);
        QJsonObject timing;
        switch (parsed.kind) {
        case ScheduleCore::AdvertTiming::Kind::Disabled: timing = {{"kind", "disabled"}}; break;
        case ScheduleCore::AdvertTiming::Kind::Frequency: timing = {{"kind", "frequency"}, {"count", parsed.frequency}}; break;
        case ScheduleCore::AdvertTiming::Kind::ExactMinutes: timing = {{"kind", "minutes"}, {"values", minutesJson(parsed.minutes)}}; break;
        default: break;
        }
        array.append(QJsonObject{{"id", r.stableId}, {"name", r.name}, {"hours", calendarJson(r.hours, 0, 23)},
                     {"weekdays", calendarJson(r.weekdays, 0, 6)}, {"from", r.from.toString(Qt::ISODate)},
                     {"until", r.until.toString(Qt::ISODate)}, {"volume", r.volume}, {"timing", timing},
                     {"preparedMinutes", minutesJson(ScheduleCore::compileAdvertMinutes(r))}});
    }
    return array;
}
bool readProject(const Paths &paths, Project *project, QByteArray *bytes, QString *error)
{
    if (!readBytes(filePath(paths), bytes, error)) return false;
    QString reason;
    if (!decode(*bytes, project, &reason)) return fail(error, filePath(paths) + QStringLiteral(": ") + reason);
    return true;
}
bool restoreDirectory(const Paths &paths, QString *error)
{
    const QString path = filePath(paths), journalPath = path + ".pending";
    if (!QFileInfo::exists(journalPath)) return true;
    QByteArray bytes;
    if (!readBytes(journalPath, &bytes, error)) return false;
    QJsonParseError parse;
    const QJsonObject journal = QJsonDocument::fromJson(bytes, &parse).object();
    const QString section = journal.value("section").toString();
    const QString from = journal.value("from").toString(), to = journal.value("to").toString();
    if (parse.error != QJsonParseError::NoError || !keys(journal, {"version", "section", "from", "to", "before", "after"})
            || journal.value("version") != QJsonValue(1) || (section != "music" && section != "video")
            || !validFileName(from, true) || !validFileName(to, true) || from == to
            || !journal.value("before").isString() || !journal.value("after").isString())
        return fail(error, QStringLiteral("Повреждён журнал %1; файлы сохранены").arg(journalPath));
    const auto before = QByteArray::fromBase64Encoding(journal.value("before").toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    const auto after = QByteArray::fromBase64Encoding(journal.value("after").toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    Project checked;
    if (!before || !after || !decode(before.decoded, &checked, error) || !decode(after.decoded, &checked, error))
        return fail(error, QStringLiteral("Повреждён снимок в журнале %1").arg(journalPath));
    QByteArray current;
    if (!readBytes(path, &current, error)) return false;
    const bool committed = current == after.decoded;
    if (!committed && current != before.decoded)
        return fail(error, QStringLiteral("Проект изменён вне незавершённой операции; журнал %1 сохранён").arg(journalPath));
    QDir root(section == "music" ? paths.musicDirectory : paths.videoDirectory);
    const QString desired = committed ? to : from, other = committed ? from : to;
    if (root.exists(desired) && root.exists(other))
        return fail(error, QStringLiteral("Конфликт восстановления каталогов %1 и %2").arg(root.filePath(from), root.filePath(to)));
    if (!root.exists(desired) && (!root.exists(other) || !root.rename(other, desired)))
        return fail(error, QStringLiteral("Не удалось восстановить каталог %1").arg(root.filePath(desired)));
    if (!QFile::remove(journalPath)) return fail(error, QStringLiteral("Не удалось удалить журнал %1").arg(journalPath));
    return true;
}
bool saveChecked(const Paths &paths, const Project &project, QString *error)
{
    const QByteArray bytes = encode(project);
    Project checked;
    if (!decode(bytes, &checked, error)) return false;
    return writeAtomic(filePath(paths), bytes, error);
}
}

QString filePath(const Paths &paths) { return QDir(paths.stationDirectory).filePath("project.json"); }
bool validFileName(const QString &name, bool channel)
{
    Q_UNUSED(channel)
    if (name.isEmpty() || name != name.trimmed() || name == "." || name == ".." || name.endsWith('.')
            || name.contains(QRegularExpression(QStringLiteral("[\\x00-\\x1f<>:\"/\\\\|?*]")))) return false;
    static const QRegularExpression reserved(QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)"), QRegularExpression::CaseInsensitiveOption);
    return !reserved.match(name).hasMatch();
}
bool decode(const QByteArray &bytes, Project *project, QString *error)
{
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, QStringLiteral("Некорректный JSON проекта: %1").arg(parse.errorString()));
    const auto root = document.object();
    if (!keys(root, {"format", "schemaVersion", "music", "video", "advert"})
            || root.value("format") != QJsonValue("mediabox.manager-project")
            || (root.value("schemaVersion") != QJsonValue(1) && root.value("schemaVersion") != QJsonValue(2)
                && root.value("schemaVersion") != QJsonValue(3)))
        return fail(error, QStringLiteral("Неподдерживаемый формат, версия или поля проекта"));
    Project result; QSet<QString> ids;
    const bool requireDirectory = root.value("schemaVersion") == QJsonValue(3);
    if (!decodeChannels(root.value("music"), &result.music, &ids, requireDirectory)) return fail(error, QStringLiteral("Некорректные музыкальные правила проекта"));
    if (!decodeChannels(root.value("video"), &result.video, &ids, requireDirectory)) return fail(error, QStringLiteral("Некорректные видеоправила проекта"));
    if (!decodeAdverts(root.value("advert"), &result.advert, &ids)) return fail(error, QStringLiteral("Некорректные рекламные правила проекта"));
    *project = result;
    return true;
}
QByteArray encode(const Project &project)
{
    return QJsonDocument(QJsonObject{{"format", "mediabox.manager-project"}, {"schemaVersion", 3},
        {"music", encodeChannels(project.music)}, {"video", encodeChannels(project.video)},
        {"advert", encodeAdverts(project.advert)}}).toJson(QJsonDocument::Indented);
}
bool load(const Paths &paths, Project *project, QString *error)
{
    const QString path = filePath(paths);
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(0)) return fail(error, QStringLiteral("Проект занят другим процессом: %1").arg(path));
    if (!restoreDirectory(paths, error)) return false;
    const QFileInfo info(path);
    if (info.exists() || info.isSymLink()) {
        QByteArray bytes;
        return readProject(paths, project, &bytes, error);
    }
    Project imported;
    if (!ScheduleImport::read(paths, &imported, error) || !saveChecked(paths, imported, error)) return false;
    *project = imported;
    return true;
}
bool replaceChannels(const Paths &paths, bool video, const QList<ScheduleCore::ChannelRule> &rules,
                     const QString &renameFrom, const QString &renameTo, QString *error)
{
    const QString path = filePath(paths);
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(0)) return fail(error, QStringLiteral("Проект занят другим процессом: %1").arg(path));
    if (!restoreDirectory(paths, error)) return false;
    Project project; QByteArray before;
    if (!readProject(paths, &project, &before, error)) return false;
    (video ? project.video : project.music) = rules;
    const QByteArray after = encode(project);
    Project checked;
    if (!decode(after, &checked, error)) return false;
    if (renameFrom.isEmpty() && renameTo.isEmpty()) return writeAtomic(path, after, error);
    if (!validFileName(renameFrom, true) || !validFileName(renameTo, true) || renameFrom == renameTo)
        return fail(error, QStringLiteral("Некорректное переименование каталога"));
    QDir root(video ? paths.videoDirectory : paths.musicDirectory);
    if (!root.exists(renameFrom) || root.exists(renameTo))
        return fail(error, QStringLiteral("Не удалось подготовить переименование %1 в %2").arg(renameFrom, renameTo));
    const QString journalPath = path + ".pending";
    const QJsonObject journal{{"version", 1}, {"section", video ? "video" : "music"}, {"from", renameFrom}, {"to", renameTo},
        {"before", QString::fromLatin1(before.toBase64())}, {"after", QString::fromLatin1(after.toBase64())}};
    if (!writeAtomic(journalPath, QJsonDocument(journal).toJson(QJsonDocument::Compact), error)) return false;
    if (!root.rename(renameFrom, renameTo)) {
        QFile::remove(journalPath);
        return fail(error, QStringLiteral("Не удалось переименовать каталог %1").arg(root.filePath(renameFrom)));
    }
    if (!writeAtomic(path, after, error)) {
        if (root.rename(renameTo, renameFrom)) QFile::remove(journalPath);
        else if (error) *error += QStringLiteral("; требуется восстановление по журналу %1").arg(journalPath);
        return false;
    }
    QFile::remove(journalPath);
    return true;
}
bool replaceAdverts(const Paths &paths, const QList<ScheduleCore::AdvertRule> &rules, QString *error)
{
    const QString path = filePath(paths);
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(0)) return fail(error, QStringLiteral("Проект занят другим процессом: %1").arg(path));
    if (!restoreDirectory(paths, error)) return false;
    Project project; QByteArray before;
    if (!readProject(paths, &project, &before, error)) return false;
    project.advert = rules;
    return saveChecked(paths, project, error);
}
}
