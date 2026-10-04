#pragma once

#include <QDateTime>
#include <QList>
#include <QSet>
#include <QStringList>

// Schedule semantics only: no widgets, item models, filesystem or system clock.
namespace ScheduleCore {

struct CalendarValues {
    QSet<int> values;
    bool valid = false;
};
CalendarValues parseCalendar(const QString &text, int minimum, int maximum);

struct AdvertTiming {
    enum class Kind { Invalid, Disabled, Frequency, ExactMinutes };
    Kind kind = Kind::Invalid;
    int frequency = 0;
    QList<int> minutes;
    QString error;
    bool valid() const { return kind != Kind::Invalid; }
};
AdvertTiming parseAdvertTiming(const QString &text);
QString formatMinutes(const QList<int> &minutes);

struct ChannelRule {
    QString name, weekdays, days, months;
    QTime start, end;
    int volume = 100;
    QString stableId;
    QString order = QStringLiteral("shuffle_cycle");
    int untilDayOffset = 0;
    // Manager storage identity; display-name edits never move media files.
    QString storageDirectory;
};

struct AdvertRule {
    // Projects use a persistent UUID; an in-memory preview may fall back to name.
    QString stableId, name, hours, weekdays;
    QDate from, until;
    QString timing;
    int volume = 100;
    // Persisted frequency phase. Empty means compile a new
    // deterministic phase; persistence supplies this only for matching rules.
    QList<int> compiledMinutes;
};

// Empty means valid. Overnight and full-day windows require an explicit day offset.
QString validateChannel(const ChannelRule &rule);
QString validateAdvert(const AdvertRule &rule);

// Reproducible across processes and platforms. Only identity and temporal
// conditions affect the phase: volume and source row order never do.
QList<int> compileAdvertMinutes(const AdvertRule &rule);

struct Channel {
    int sourceRow = -1;
    QString name, weekdays, days, months, reason, status;
    QTime start, end;
    int volume = 0;
    bool valid = false;
    bool calendarMatches = false;
    bool active = false;
    int untilDayOffset = 0;
    // Visible portions on the preview date, in minutes [0, 1440].
    QList<QPair<int, int>> dayIntervals;
};

struct Snapshot {
    QDateTime at;
    int horizonDays = 0;
    QList<Channel> channels;
    QList<int> activeRows;
    QStringList issues;
    QDateTime nextChannelTime, nextAdvertTime;
    QStringList nextChannelNames, nextAdvertNames;
    QStringList exactAdvertsNow, frequencyAdvertsNow;
    QString currentSummary, nextChannelSummary, nextAdvertSummary;
    bool hasUnresolvedRules = false;
};

Snapshot evaluate(const QList<ChannelRule> &channels, const QList<AdvertRule> &adverts,
                  const QDateTime &at, int horizonDays = 366);
} // namespace ScheduleCore
