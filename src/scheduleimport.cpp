#include "scheduleimport.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QUuid>

namespace ScheduleImport {
namespace {
bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}
bool lines(const QString &path, QStringList *result, QString *error)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) { result->clear(); return true; }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, QStringLiteral("Не удалось прочитать %1: %2").arg(path, file.errorString()));
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) return fail(error, QStringLiteral("Ошибка чтения %1: %2").arg(path, file.errorString()));
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder(bytes);
    if (decoder.hasError()) return fail(error, QStringLiteral("%1: некорректный UTF-8").arg(path));
    *result = text.split('\n');
    if (!result->isEmpty() && result->last().isEmpty()) result->removeLast();
    for (QString &line : *result) if (line.endsWith('\r')) line.chop(1);
    return true;
}
bool lineError(const QString &path, int line, const QString &reason, QString *error)
{
    return fail(error, QStringLiteral("%1, строка %2: %3").arg(path).arg(line).arg(reason));
}
bool volume(const QString &text, int *result)
{
    static const QRegularExpression integer(QStringLiteral("^[0-9]{1,3}$"));
    bool ok = false; *result = text.toInt(&ok);
    return ok && integer.match(text).hasMatch() && *result <= 100;
}
bool channels(const QString &path, QList<ScheduleCore::ChannelRule> *result, QString *error)
{
    QStringList input;
    if (!lines(path, &input, error)) return false;
    QSet<QString> names;
    for (int row = 0; row < input.size(); ++row) {
        const auto f = input[row].split(' ');
        if (f.size() != 7) return lineError(path, row + 1, QStringLiteral("ожидаются семь полей"), error);
        ScheduleCore::ChannelRule r;
        r.name = f[0]; r.start = QTime::fromString(f[1], "HH:mm"); r.end = QTime::fromString(f[2], "HH:mm");
        r.weekdays = f[3]; r.days = f[4]; r.months = f[5];
        if (!ProjectRepository::validFileName(r.name, true) || names.contains(r.name.toCaseFolded())
                || r.start.toString("HH:mm") != f[1] || r.end.toString("HH:mm") != f[2] || !volume(f[6], &r.volume))
            return lineError(path, row + 1, QStringLiteral("некорректное имя, время, громкость или повтор канала"), error);
        const QString reason = ScheduleCore::validateChannel(r);
        if (!reason.isEmpty()) return lineError(path, row + 1, reason, error);
        r.stableId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        r.storageDirectory = r.name;
        names.insert(r.name.toCaseFolded()); result->append(r);
    }
    return true;
}
bool adverts(const QString &path, bool generated, QList<ScheduleCore::AdvertRule> *result, QString *error)
{
    QStringList input;
    if (!lines(path, &input, error)) return false;
    // Older network Managers copied the eight-column generated table into
    // advertView. Accept that complete shape as exact/disabled imported rules.
    if (!generated && !input.isEmpty() && input.first().split(';').size() == 8) generated = true;
    for (int row = 0; row < input.size(); ++row) {
        const auto f = input[row].split(';');
        if (f.size() != (generated ? 8 : 7)) return lineError(path, row + 1, QStringLiteral("неверное число полей рекламного правила"), error);
        ScheduleCore::AdvertRule r;
        r.name = f[0]; r.hours = f[1]; r.timing = f[2]; r.weekdays = f[3];
        r.from = QDate::fromString(f[4], "dd.MM.yyyy"); r.until = QDate::fromString(f[5], "dd.MM.yyyy");
        if (!ProjectRepository::validFileName(r.name, false) || r.from.toString("dd.MM.yyyy") != f[4]
                || r.until.toString("dd.MM.yyyy") != f[5] || !volume(f[6], &r.volume))
            return lineError(path, row + 1, QStringLiteral("некорректное имя, дата или громкость"), error);
        const QString reason = ScheduleCore::validateAdvert(r);
        if (!reason.isEmpty()) return lineError(path, row + 1, reason, error);
        if (generated) {
            bool orderOk = false; const int order = f[7].toInt(&orderOk);
            const auto kind = ScheduleCore::parseAdvertTiming(r.timing).kind;
            if (!orderOk || order != 0
                    || (kind != ScheduleCore::AdvertTiming::Kind::ExactMinutes && kind != ScheduleCore::AdvertTiming::Kind::Disabled))
                return lineError(path, row + 1, QStringLiteral("некорректные минуты или порядок результата генерации"), error);
        }
        r.stableId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        result->append(r);
    }
    return true;
}
bool calendarMatches(const ScheduleCore::AdvertRule &a, const ScheduleCore::AdvertRule &b)
{
    return a.name == b.name && a.from == b.from && a.until == b.until
            && ScheduleCore::parseCalendar(a.hours, 0, 23).values == ScheduleCore::parseCalendar(b.hours, 0, 23).values
            && ScheduleCore::parseCalendar(a.weekdays, 0, 6).values == ScheduleCore::parseCalendar(b.weekdays, 0, 6).values;
}
}

bool read(const ProjectRepository::Paths &paths, ProjectRepository::Project *project, QString *error)
{
    const QDir root(paths.stationDirectory);
    ProjectRepository::Project imported;
    if (!channels(root.filePath("timetable/timetable"), &imported.music, error)
            || !channels(root.filePath("timetable/vtimetable"), &imported.video, error)) return false;
    const QString source = root.filePath("timetable/advertView"), derived = root.filePath("timetable/adverttable");
    const QFileInfo sourceInfo(source);
    const bool generatedOnly = !sourceInfo.exists() && !sourceInfo.isSymLink();
    if (!adverts(generatedOnly ? derived : source, generatedOnly, &imported.advert, error)) return false;
    QList<ScheduleCore::AdvertRule> generated;
    if (!generatedOnly && !adverts(derived, true, &generated, error)) return false;
    QSet<int> used;
    for (auto &rule : imported.advert) {
        const auto kind = ScheduleCore::parseAdvertTiming(rule.timing).kind;
        if (kind == ScheduleCore::AdvertTiming::Kind::Disabled) continue;
        for (int index = 0; index < generated.size(); ++index) {
            if (used.contains(index) || !calendarMatches(rule, generated[index])) continue;
            if (ScheduleCore::parseAdvertTiming(generated[index].timing).kind == ScheduleCore::AdvertTiming::Kind::Disabled) continue;
            // Consume exact source rows too: otherwise a later frequency rule
            // for the same asset/calendar would steal this row's exact phase.
            used.insert(index);
            if (kind != ScheduleCore::AdvertTiming::Kind::Frequency) break;
            const auto phase = ScheduleCore::compileAdvertMinutes(generated[index]);
            auto checked = rule; checked.compiledMinutes = phase;
            if (ScheduleCore::validateAdvert(checked).isEmpty()) rule.compiledMinutes = phase;
            break;
        }
        if (kind == ScheduleCore::AdvertTiming::Kind::Frequency && rule.compiledMinutes.isEmpty())
            rule.compiledMinutes = ScheduleCore::compileAdvertMinutes(rule);
    }
    *project = imported;
    return true;
}
}
