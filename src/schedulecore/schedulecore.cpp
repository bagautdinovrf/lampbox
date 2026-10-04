#include "schedulecore.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTimeZone>
#include <algorithm>
#include <utility>

namespace {
bool validPublishedMinutes(const ScheduleCore::AdvertRule &rule, const ScheduleCore::AdvertTiming &timing)
{
    if (rule.compiledMinutes.isEmpty())
        return true;
    if (timing.kind != ScheduleCore::AdvertTiming::Kind::Frequency
            || rule.compiledMinutes.size() != timing.frequency)
        return false;
    const int step = 60 / timing.frequency;
    if (rule.compiledMinutes.first() < 0 || rule.compiledMinutes.first() >= step
            || rule.compiledMinutes.last() > 59)
        return false;
    for (int index = 1; index < rule.compiledMinutes.size(); ++index)
        if (rule.compiledMinutes.at(index) - rule.compiledMinutes.at(index - 1) != step)
            return false;
    return true;
}
}

ScheduleCore::AdvertTiming ScheduleCore::parseAdvertTiming(const QString &text)
{
    AdvertTiming result;
    if (text == QLatin1String("*")) {
        result.kind = AdvertTiming::Kind::Disabled;
        return result;
    }
    static const QRegularExpression integer(QStringLiteral("^[0-9]+$"));
    if (integer.match(text).hasMatch()) {
        bool ok = false;
        const int frequency = text.toInt(&ok);
        if (ok && frequency >= 1 && frequency <= 5) {
            result.kind = AdvertTiming::Kind::Frequency;
            result.frequency = frequency;
            return result;
        }
        result.error = QStringLiteral("Частота должна быть от 1 до 5 выходов в час.");
        return result;
    }
    static const QRegularExpression exact(QStringLiteral("^([0-9]{1,2})m$"));
    QSet<int> minutes;
    for (const QString &part : text.split(QLatin1Char(','))) {
        const auto match = exact.match(part);
        if (!match.hasMatch() || match.captured(1).toInt() > 59) {
            result.error = QStringLiteral("Некорректные минуты: ожидаются 00m,30m, частота 1–5 или * (никогда).");
            return result;
        }
        minutes.insert(match.captured(1).toInt());
    }
    result.kind = AdvertTiming::Kind::ExactMinutes;
    result.minutes = minutes.values();
    std::sort(result.minutes.begin(), result.minutes.end());
    return result;
}

QString ScheduleCore::formatMinutes(const QList<int> &minutes)
{
    QStringList text;
    for (int minute : minutes)
        text.append(QString::number(minute) + QLatin1Char('m'));
    return text.join(QLatin1Char(','));
}

QString ScheduleCore::validateChannel(const ChannelRule &rule)
{
    if (rule.order != QStringLiteral("sequential") && rule.order != QStringLiteral("shuffle_cycle"))
        return QStringLiteral("Выберите порядок треков: по порядку или случайно.");
    if (!parseCalendar(rule.weekdays, 0, 6).valid || !parseCalendar(rule.days, 1, 31).valid
            || !parseCalendar(rule.months, 1, 12).valid)
        return QStringLiteral("Некорректное календарное условие: ожидаются * или числа через запятую.");
    if (!rule.start.isValid() || !rule.end.isValid())
        return QStringLiteral("Некорректное время интервала.");
    if (rule.untilDayOffset < 0 || rule.untilDayOffset > 1
            || (rule.untilDayOffset == 1 && rule.end > rule.start))
        return QStringLiteral("Интервал должен длиться не более суток; проверьте время и день окончания.");
    if (rule.untilDayOffset == 0 && rule.start >= rule.end)
        return rule.start == rule.end
                ? QStringLiteral("Начало совпадает с окончанием: включите «Полные сутки» или укажите следующий день")
                : QStringLiteral("Переход через полночь: укажите окончание на следующий день");
    if (rule.volume < 0 || rule.volume > 100)
        return QStringLiteral("Громкость должна быть от 0 до 100%.");
    return {};
}

QString ScheduleCore::validateAdvert(const AdvertRule &rule)
{
    const auto timing = parseAdvertTiming(rule.timing);
    if (!timing.valid())
        return timing.error;
    if (!validPublishedMinutes(rule, timing))
        return QStringLiteral("Сохранённые минуты не соответствуют частоте рекламного правила.");
    if (!parseCalendar(rule.hours, 0, 23).valid || !parseCalendar(rule.weekdays, 0, 6).valid)
        return QStringLiteral("Некорректное рекламное календарное условие: часы 0–23 и дни недели 0–6.");
    if (!rule.from.isValid() || !rule.until.isValid() || rule.from > rule.until)
        return QStringLiteral("Некорректный период рекламы: проверьте даты начала и окончания.");
    if (rule.volume < 0 || rule.volume > 100)
        return QStringLiteral("Громкость должна быть от 0 до 100%.");
    return {};
}

QList<int> ScheduleCore::compileAdvertMinutes(const AdvertRule &rule)
{
    const auto timing = parseAdvertTiming(rule.timing);
    if (!validPublishedMinutes(rule, timing))
        return {};
    if (!rule.compiledMinutes.isEmpty())
        return rule.compiledMinutes;
    if (timing.kind == AdvertTiming::Kind::ExactMinutes)
        return timing.minutes;
    if (timing.kind != AdvertTiming::Kind::Frequency)
        return {};
    const auto canonicalCalendar = [](const QString &text, int first, int last) {
        const auto parsed = parseCalendar(text, first, last);
        if (!parsed.valid)
            return text; // Validation belongs to the complete-rule boundary.
        auto ordered = parsed.values.values();
        std::sort(ordered.begin(), ordered.end());
        QStringList parts;
        for (int value : ordered)
            parts.append(QString::number(value));
        return parts.join(QLatin1Char(','));
    };
    // JSON's length-delimited strings avoid ambiguous concatenation. SHA-256
    // is stable, unlike Qt's process-seeded qHash or a global random generator.
    const QJsonArray identity{rule.stableId.isEmpty() ? rule.name : rule.stableId,
                              canonicalCalendar(rule.hours, 0, 23),
                              canonicalCalendar(rule.weekdays, 0, 6),
                              rule.from.toString(Qt::ISODate), rule.until.toString(Qt::ISODate),
                              timing.frequency};
    const auto digest = QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact),
                                                QCryptographicHash::Sha256);
    quint32 phase = 0;
    for (int index = 0; index < 4; ++index)
        phase = (phase << 8) | static_cast<unsigned char>(digest.at(index));
    const int step = 60 / timing.frequency;
    const int first = int(phase % quint32(step));
    QList<int> minutes;
    for (int index = 0; index < timing.frequency; ++index)
        minutes.append(first + index * step);
    return minutes;
}

ScheduleCore::CalendarValues ScheduleCore::parseCalendar(const QString &text, int minimum, int maximum)
{
    ScheduleCore::CalendarValues result;
    if (text == QLatin1String("*")) {
        for (int number = minimum; number <= maximum; ++number)
            result.values.insert(number);
        result.valid = true;
        return result;
    }
    // Editors persist expanded comma-separated numbers. Do not silently
    // reinterpret unsupported cron expressions, empty fields or partial input.
    static const QRegularExpression number(QStringLiteral("^[0-9]+$"));
    for (const QString &part : text.split(QLatin1Char(','))) {
        bool ok = false;
        const int parsed = part.toInt(&ok);
        if (!number.match(part).hasMatch() || !ok || parsed < minimum || parsed > maximum)
            return {};
        result.values.insert(parsed);
    }
    result.valid = !result.values.isEmpty();
    return result;
}


namespace {
QDateTime onDate(const QDateTime &at, const QDate &date, const QTime &time)
{
    // Match the published v1 contract: skip gaps and select the first occurrence
    // of a repeated local time. Reject normalization across a missing wall time.
    const QDateTime resolved(date, time, at.timeZone(), QDateTime::TransitionResolution::PreferBefore);
    return resolved.isValid() && resolved.date() == date && resolved.time() == time ? resolved : QDateTime{};
}

QString eventLabel(const QDateTime &when, const QStringList &names)
{
    return when.toString(QStringLiteral("dd.MM · HH:mm")) + QStringLiteral(" · ") + names.join(QStringLiteral(", "));
}

struct ChannelCalendar {
    ScheduleCore::CalendarValues weekdays, days, months;
    bool matches(const QDate &date) const
    {
        // QDate is Monday=1...Sunday=7; the persisted format is Sunday=0.
        return date.isValid() && weekdays.values.contains(date.dayOfWeek() % 7)
                && days.values.contains(date.day()) && months.values.contains(date.month());
    }
    QString exclusion(const QDate &date) const
    {
        if (!weekdays.values.contains(date.dayOfWeek() % 7))
            return QStringLiteral("День недели исключён из расписания");
        if (!months.values.contains(date.month()))
            return QStringLiteral("Месяц исключён из расписания");
        return QStringLiteral("День месяца исключён из расписания");
    }
};
}


ScheduleCore::Snapshot ScheduleCore::evaluate(const QList<ChannelRule> &channels,
                                                    const QList<AdvertRule> &adverts,
                                                    const QDateTime &at, int horizonDays)
{
    Snapshot result;
    result.at = at;
    result.horizonDays = std::clamp(horizonDays, 1, 366);
    if (!at.isValid()) {
        result.currentSummary = QStringLiteral("Некорректная дата или время просмотра");
        result.issues.append(result.currentSummary);
        return result;
    }

    QList<ChannelCalendar> calendars;
    for (int row = 0; row < channels.size(); ++row) {
        Channel channel;
        channel.sourceRow = row;
        const auto &rule = channels.at(row);
        channel.name = rule.name;
        channel.start = rule.start;
        channel.end = rule.end;
        channel.weekdays = rule.weekdays;
        channel.days = rule.days;
        channel.months = rule.months;
        channel.volume = rule.volume;
        const ChannelCalendar calendar{parseCalendar(channel.weekdays, 0, 6), parseCalendar(channel.days, 1, 31), parseCalendar(channel.months, 1, 12)};
        channel.untilDayOffset = rule.untilDayOffset;
        const QString validationError = validateChannel(rule);
        channel.valid = validationError.isEmpty();
        bool localWindowSupported = true;
        // A night window belongs to its start date. Check yesterday before
        // today's filter so month, year and weekday boundaries retain the tail.
        if (channel.valid) {
            for (int offset = -rule.untilDayOffset; offset <= 0; ++offset) {
                const QDate anchor = at.date().addDays(offset);
                if (!calendar.matches(anchor)) continue;
                const QDateTime from = onDate(at, anchor, rule.start);
                const QDateTime until = onDate(at, anchor.addDays(rule.untilDayOffset), rule.end);
                if (!from.isValid() || !until.isValid()) {
                    localWindowSupported = false;
                    continue;
                }
                const int first = std::max(0, offset * 1440 + rule.start.hour() * 60 + rule.start.minute());
                const int last = std::min(1440, (offset + rule.untilDayOffset) * 1440 + rule.end.hour() * 60 + rule.end.minute());
                if (first < last) channel.dayIntervals.append({first, last});
                if (at >= from && at < until) channel.active = true;
            }
        }
        channel.calendarMatches = !channel.dayIntervals.isEmpty();
        if (!validationError.isEmpty()) {
            channel.reason = validationError;
        } else if (!localWindowSupported) {
            channel.reason = QStringLiteral("Локальное время начала или окончания отсутствует при переводе часов; окно не включено в расчёт");
        } else if (channel.active) {
            channel.reason = QStringLiteral("Календарные условия дня начала и интервал совпали. Это расчёт плана, не подтверждение воспроизведения");
        } else if (!channel.calendarMatches) {
            channel.reason = calendar.exclusion(at.date());
        } else if (at.time() < channel.start && calendar.matches(at.date())) {
            channel.reason = QStringLiteral("Окно ещё не началось · сегодня с ") + channel.start.toString(QStringLiteral("HH:mm"));
        } else {
            channel.reason = QStringLiteral("Окно на сегодня закончилось в ") + channel.end.toString(QStringLiteral("HH:mm"));
        }
        if (channel.active) result.activeRows.append(row);
        channel.status = !channel.valid || !localWindowSupported ? QStringLiteral("Требует проверки")
                : channel.active ? QStringLiteral("По расписанию")
                : !channel.calendarMatches ? QStringLiteral("Не в этот день")
                : at.time() < channel.start && calendar.matches(at.date()) ? QStringLiteral("Позже") : QStringLiteral("Завершён");
        if (!channel.valid || !localWindowSupported) {
            result.hasUnresolvedRules = true;
            result.issues.append(channel.name + QStringLiteral(": ") + channel.reason);
        }
        result.channels.append(channel);
        calendars.append(calendar);
    }

    if (result.activeRows.size() > 1) {
        QStringList active;
        for (int row : result.activeRows)
            active.append(result.channels.at(row).name);
        result.currentSummary = QStringLiteral("Пересечение: ") + active.join(QStringLiteral(", "));
        result.issues.prepend(QStringLiteral("Подходят несколько каналов. Приоритет не задан: ") + active.join(QStringLiteral(", ")));
    } else if (result.activeRows.size() == 1) {
        result.currentSummary = QStringLiteral("По расписанию · ") + result.channels.at(result.activeRows.first()).name;
    } else {
        result.currentSummary = QStringLiteral("Нет подходящего канала");
    }
    if (result.hasUnresolvedRules)
        result.currentSummary += QStringLiteral(" · есть непроверенные условия");

    // Detect overlaps and internal gaps with a sweep, rather than comparing
    // every pair of channels. The view remains useful for large channel lists.
    QList<QPair<int, int>> boundaries;
    for (const auto &channel : std::as_const(result.channels)) {
        if (channel.valid) {
            for (const auto &span : channel.dayIntervals) {
                boundaries.append({span.first, 1});
                boundaries.append({span.second, -1});
            }
        }
    }
    std::sort(boundaries.begin(), boundaries.end());
    int activeCount = 0;
    bool overlapReported = result.activeRows.size() > 1;
    bool gapReported = false;
    for (qsizetype index = 0; index < boundaries.size();) {
        const int minute = boundaries.at(index).first;
        while (index < boundaries.size() && boundaries.at(index).first == minute)
            activeCount += boundaries.at(index++).second;
        if (index == boundaries.size())
            break;
        const QString span = QTime(minute / 60, minute % 60).toString(QStringLiteral("HH:mm"))
                + QStringLiteral("–") + (boundaries.at(index).first == 1440 ? QStringLiteral("24:00") : QTime(boundaries.at(index).first / 60, boundaries.at(index).first % 60).toString(QStringLiteral("HH:mm")));
        if (activeCount > 1 && !overlapReported) {
            result.issues.append(span + QStringLiteral(": пересечение каналов, приоритет не задан"));
            overlapReported = true;
        }
        if (activeCount == 0 && !gapReported) {
            result.issues.append(span + QStringLiteral(": между интервалами нет назначенного канала"));
            gapReported = true;
        }
    }

    // A change can be an end as well as a start (including a transition to a
    // gap). Group simultaneous boundaries; do not arbitrarily choose a winner.
    for (int day = -1; day < result.horizonDays; ++day) {
        const QDate date = at.date().addDays(day);
        if (!date.isValid() || (result.nextChannelTime.isValid() && date > result.nextChannelTime.date()))
            break;
        for (qsizetype row = 0; row < result.channels.size(); ++row) {
            const auto &channel = result.channels.at(row);
            if (!channel.valid || !calendars.at(row).matches(date))
                continue;
            const QDateTime from = onDate(at, date, channel.start);
            const QDateTime until = onDate(at, date.addDays(channel.untilDayOffset), channel.end);
            if (!from.isValid() || !until.isValid()) continue;
            for (const QDateTime &candidate : {from, until}) {
                if (!candidate.isValid() || candidate <= at)
                    continue;
                const QString name = channel.name + (candidate == from ? QStringLiteral(" — начало") : QStringLiteral(" — окончание"));
                if (!result.nextChannelTime.isValid() || candidate < result.nextChannelTime) {
                    result.nextChannelTime = candidate;
                    result.nextChannelNames = {name};
                } else if (candidate == result.nextChannelTime) {
                    result.nextChannelNames.append(name);
                }
            }
        }
    }

    for (const auto &rule : adverts) {
        const QString name = rule.name;
        const QString error = validateAdvert(rule);
        if (!error.isEmpty()) {
            result.issues.append(name + QStringLiteral(": ") + error);
            result.hasUnresolvedRules = true;
            continue;
        }
        const auto timing = parseAdvertTiming(rule.timing);
        if (timing.kind == AdvertTiming::Kind::Disabled)
            continue;
        const auto hours = parseCalendar(rule.hours, 0, 23);
        const auto weekdays = parseCalendar(rule.weekdays, 0, 6);
        const auto from = rule.from, until = rule.until;
        const auto eligible = [&](const QDate &date) {
            return date >= from && date <= until && weekdays.values.contains(date.dayOfWeek() % 7);
        };
        const auto minutes = compileAdvertMinutes(rule);
        if (timing.kind == AdvertTiming::Kind::Frequency && eligible(at.date())
                && hours.values.contains(at.time().hour()))
            result.frequencyAdvertsNow.append(name + QStringLiteral(" · %1 в час · ").arg(timing.frequency)
                                             + formatMinutes(minutes));
        const QDateTime scheduledMinute = onDate(at, at.date(), QTime(at.time().hour(), at.time().minute()));
        if (eligible(at.date()) && hours.values.contains(at.time().hour()) && minutes.contains(at.time().minute())
                && scheduledMinute.isValid() && at >= scheduledMinute && at < scheduledMinute.addSecs(60))
            result.exactAdvertsNow.append(name);
        QList<int> dailyMinutes;
        for (int hour : hours.values)
            for (int minute : minutes)
                dailyMinutes.append(hour * 60 + minute);
        std::sort(dailyMinutes.begin(), dailyMinutes.end());
        for (int day = 0; day < result.horizonDays; ++day) {
            const QDate date = at.date().addDays(day);
            if (!date.isValid() || date > until || (result.nextAdvertTime.isValid() && date > result.nextAdvertTime.date()))
                break;
            if (!eligible(date))
                continue;
            bool found = false;
            for (int minute : std::as_const(dailyMinutes)) {
                const QDateTime candidate = onDate(at, date, QTime(minute / 60, minute % 60));
                if (!candidate.isValid() || candidate <= at)
                    continue;
                if (!result.nextAdvertTime.isValid() || candidate < result.nextAdvertTime) {
                    result.nextAdvertTime = candidate;
                    result.nextAdvertNames = {name};
                } else if (candidate == result.nextAdvertTime) {
                    result.nextAdvertNames.append(name);
                }
                found = true;
                break;
            }
            if (found)
                break;
        }
    }
    result.nextChannelSummary = result.nextChannelTime.isValid()
            ? eventLabel(result.nextChannelTime, result.nextChannelNames)
            : QStringLiteral("Не найдена за %1 дней").arg(result.horizonDays);
    result.nextAdvertSummary = result.nextAdvertTime.isValid()
            ? eventLabel(result.nextAdvertTime, result.nextAdvertNames)
            : QStringLiteral("Не найден за %1 дней").arg(result.horizonDays);
    return result;
}
