#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QSharedPointer>
#include <QStringList>

// The published schedule contract. Pure temporal semantics; no files or audio.
namespace ScheduleV1 {
struct Compiled;
struct Document {
    QJsonObject object;
    QSharedPointer<const Compiled> compiled;
    QStringList diagnostics; // Non-fatal skipped DST occurrences in validity.
    QString scheduleId() const { return object.value("scheduleId").toString(); }
    QString stationId() const { return object.value("stationId").toString(); }
    QString publicationId() const { return object.value("publicationId").toString(); }
    int revision() const { return object.value("revision").toInt(); }
};

struct Evaluation {
    bool withinValidity = false;
    bool usingFallback = true;
    QString baseRuleId, baseSlotId, playlistId;
    bool silence = true;
    int volumePercent = 100;
    QString mixRuleId;
    QJsonArray pattern;
    // Start of the uninterrupted winning mix rule, including across midnight.
    QDateTime activationStart;
    QStringList diagnostics;
};

struct EventOccurrence {
    QString ruleId, assetId;
    QDateTime scheduledUtc;
    int priority = 0;
    int volumePercent = 100;
    int maxLateSeconds = 0;
    QString start; // after_track or interrupt
};

// Empty return value means success. Result is replaced only after full validation.
// parse additionally rejects invalid UTF-8, a BOM and duplicate JSON object keys.
QString parse(const QByteArray &bytes, Document *result);
QString decode(const QJsonObject &object, Document *result);
QString strictJsonObject(const QByteArray &bytes, QJsonObject *result);
QStringList requiredCapabilities(const QJsonObject &object);
Evaluation evaluate(const Document &document, const QDateTime &at);
QList<EventOccurrence> events(const Document &document, const QDateTime &fromInclusive,
                             const QDateTime &toInclusive);
} // namespace ScheduleV1
