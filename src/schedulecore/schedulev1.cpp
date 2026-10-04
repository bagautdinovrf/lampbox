#include "schedulev1.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringConverter>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <limits>
#include <iterator>
#include <map>
#include <set>

namespace ScheduleV1 {
namespace {
struct Invalid { QString text; };
[[noreturn]] void fail(const QString &where, const QString &why)
{ throw Invalid{where + QStringLiteral(": ") + why}; }

class JsonKeys {
public:
    explicit JsonKeys(const QByteArray &data) : data(data) {}
    void check() { value(); }
private:
    const QByteArray &data;
    qsizetype pos = 0;
    void space() { while (pos < data.size() && (data[pos] == ' ' || data[pos] == '\r' || data[pos] == '\n' || data[pos] == '\t')) ++pos; }
    QString string()
    {
        const qsizetype begin = pos++;
        while (pos < data.size()) {
            const char ch = data[pos++];
            if (ch == '\\') ++pos;
            else if (ch == '"') break;
        }
        return QJsonDocument::fromJson(QByteArray("[") + data.mid(begin, pos - begin) + ']').array().first().toString();
    }
    void value()
    {
        space();
        if (data[pos] == '"') { string(); return; }
        if (data[pos] == '{') {
            ++pos; space(); QSet<QString> keys;
            if (data[pos] == '}') { ++pos; return; }
            for (;;) {
                space(); const QString key = string();
                if (keys.contains(key)) fail(QStringLiteral("JSON"), QStringLiteral("Повторяющийся ключ «%1»").arg(key));
                keys.insert(key); space(); ++pos; value(); space();
                if (data[pos++] == '}') return;
            }
        }
        if (data[pos] == '[') {
            ++pos; space();
            if (data[pos] == ']') { ++pos; return; }
            for (;;) { value(); space(); if (data[pos++] == ']') return; }
        }
        while (pos < data.size() && data[pos] != ',' && data[pos] != ']' && data[pos] != '}'
               && data[pos] != ' ' && data[pos] != '\r' && data[pos] != '\n' && data[pos] != '\t') ++pos;
    }
};

void fields(const QJsonObject &o, const QString &p, std::initializer_list<const char *> required,
            std::initializer_list<const char *> optional = {})
{
    QSet<QString> allowed;
    for (const auto *key : required) {
        allowed.insert(QLatin1String(key));
        if (!o.contains(QLatin1String(key))) fail(p, QStringLiteral("Отсутствует поле %1").arg(QLatin1String(key)));
    }
    for (const auto *key : optional) allowed.insert(QLatin1String(key));
    for (auto it = o.begin(); it != o.end(); ++it)
        if (!allowed.contains(it.key())) fail(p, QStringLiteral("Неизвестное поле %1").arg(it.key()));
}
QJsonObject object(const QJsonValue &v, const QString &p)
{ if (!v.isObject()) fail(p, QStringLiteral("Ожидается объект")); return v.toObject(); }
QJsonArray array(const QJsonValue &v, const QString &p, int minimum = 0, int maximum = std::numeric_limits<int>::max())
{
    if (!v.isArray() || v.toArray().size() < minimum || v.toArray().size() > maximum)
        fail(p, QStringLiteral("Некорректный массив"));
    return v.toArray();
}
QString string(const QJsonValue &v, const QString &p)
{
    if (!v.isString() || v.toString().isEmpty() || v.toString().contains(QChar::Null))
        fail(p, QStringLiteral("Ожидается непустая строка без NUL"));
    return v.toString();
}
int integer(const QJsonValue &v, const QString &p, int minimum, int maximum)
{
    if (!v.isDouble() || !std::isfinite(v.toDouble()) || std::floor(v.toDouble()) != v.toDouble()
        || v.toDouble() < minimum || v.toDouble() > maximum) fail(p, QStringLiteral("Число вне допустимого целого диапазона"));
    return int(v.toDouble());
}
void constant(const QJsonValue &v, const QString &p, const QString &expected)
{ if (!v.isString() || v.toString() != expected) fail(p, QStringLiteral("Ожидается %1").arg(expected)); }
QDate date(const QJsonValue &v, const QString &p)
{
    const auto text = string(v, p); const auto parsed = QDate::fromString(text, Qt::ISODate);
    if (text.size() != 10 || !parsed.isValid() || parsed.year() < 1 || parsed.toString(Qt::ISODate) != text)
        fail(p, QStringLiteral("Ожидается реальная дата YYYY-MM-DD"));
    return parsed;
}
QTime time(const QJsonValue &v, const QString &p)
{
    const auto text = string(v, p); const auto parsed = QTime::fromString(text, QStringLiteral("HH:mm:ss"));
    if (!parsed.isValid() || parsed.toString(QStringLiteral("HH:mm:ss")) != text)
        fail(p, QStringLiteral("Ожидается время HH:mm:ss"));
    return parsed;
}
QString uuid(const QJsonValue &v, const QString &p)
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
    const auto text = string(v, p);
    if (!re.match(text).hasMatch()) fail(p, QStringLiteral("Ожидается UUID в строчной записи"));
    return text;
}
QPair<QDate,QDate> range(const QJsonValue &v, const QString &p)
{
    const auto o = object(v, p); fields(o, p, {"from", "until"});
    const auto from = date(o.value("from"), p + ".from"), until = date(o.value("until"), p + ".until");
    if (from >= until) fail(p, QStringLiteral("Диапазон должен иметь положительную длительность"));
    return {from, until};
}
void dates(const QJsonValue &v, const QString &p, int minimum = 0)
{
    QSet<QDate> seen;
    for (const auto &value : array(v, p, minimum)) {
        const auto d = date(value, p);
        if (seen.contains(d)) fail(p, QStringLiteral("Повторяющаяся дата"));
        seen.insert(d);
    }
}
void numbers(const QJsonValue &v, const QString &p, int minimum, int maximum)
{
    QSet<int> seen;
    for (const auto &value : array(v, p, 1)) {
        const int n = integer(value, p, minimum, maximum);
        if (seen.contains(n)) fail(p, QStringLiteral("Повторяющееся число"));
        seen.insert(n);
    }
}
QPair<int,int> window(const QJsonValue &v, const QString &p)
{
    const auto o = object(v, p); fields(o, p, {"from", "until", "untilDayOffset"});
    const int from = QTime(0,0).secsTo(time(o.value("from"), p + ".from"));
    const int until = QTime(0,0).secsTo(time(o.value("until"), p + ".until"))
        + 86400 * integer(o.value("untilDayOffset"), p + ".untilDayOffset", 0, 1);
    if (until <= from || until - from > 86400) fail(p, QStringLiteral("Длительность окна должна быть от одной секунды до суток"));
    return {from, until};
}
void noWindowOverlap(QList<QPair<int,int>> windows, const QString &p)
{
    std::sort(windows.begin(), windows.end());
    int until = -1;
    for (const auto &w : windows) {
        if (w.first < until) fail(p, QStringLiteral("Пересекающиеся окна"));
        until = w.second;
    }
}

struct Segment {
    qint64 from = 0, until = 0;
    int priority = 0;
    QJsonObject rule, slot;
};
struct Diagnostic { QDate date; QString text; };
}

struct Compiled {
    QTimeZone zone;
    QDate fromDate, untilDate;
    qint64 from = 0, until = 0;
    QList<Segment> base, mix;
    QJsonArray eventRules;
    QHash<QString,QJsonObject> calendars;
    QList<Diagnostic> diagnostics;
};

namespace {
class Validator {
public:
    QHash<QString,QJsonObject> assets, playlists, calendars, templates;
    QSet<QString> ids, usedCalendars;
    struct Reference { QString type, id, path; };
    QList<Reference> references;
    QPair<QDate,QDate> validity;
    QTimeZone zone;
    void id(const QJsonObject &o, const QString &p, const QString &key = QStringLiteral("id"))
    {
        const auto value = uuid(o.value(key), p + '.' + key);
        if (ids.contains(value)) fail(p, QStringLiteral("Повторяющийся ID %1").arg(value));
        ids.insert(value);
    }
    void ref(const QJsonValue &v, const QString &p, const QString &type)
    { references.append({type, uuid(v, p), p}); }
    void source(const QJsonValue &v, const QString &p, bool mix = false)
    {
        const auto o = object(v,p); const auto type = string(o.value("type"),p + ".type");
        if (type == "playlist") { fields(o,p,{"type","playlistId"}); ref(o.value("playlistId"),p + ".playlistId","playlist"); }
        else if (type == (mix ? "active_base" : "silence")) fields(o,p,{"type"});
        else fail(p,QStringLiteral("Неподдерживаемый источник"));
    }
    void condition(const QJsonValue &v, const QString &p)
    {
        const auto o = object(v,p); fields(o,p,{"select","excludeDates"},{"range"});
        dates(o.value("excludeDates"),p + ".excludeDates");
        if (o.contains("range")) range(o.value("range"),p + ".range");
        const auto s = object(o.value("select"),p + ".select");
        const auto type = string(s.value("type"),p + ".select.type");
        if (type == "all") fields(s,p,{"type"});
        else if (type == "weekdays") { fields(s,p,{"type","days"}); numbers(s.value("days"),p + ".days",1,7); }
        else if (type == "dates") { fields(s,p,{"type","dates"}); dates(s.value("dates"),p + ".dates",1); }
        else if (type == "calendar") {
            fields(s,p,{"type","calendarId"}); ref(s.value("calendarId"),p + ".calendarId","calendar");
            usedCalendars.insert(s.value("calendarId").toString());
        } else if (type == "annual_filter") {
            fields(s,p,{"type","months","monthDays","weekdays"});
            numbers(s.value("months"),p + ".months",1,12); numbers(s.value("monthDays"),p + ".monthDays",1,31);
            numbers(s.value("weekdays"),p + ".weekdays",1,7);
        } else fail(p,QStringLiteral("Неподдерживаемое календарное условие"));
    }
    void rule(const QJsonObject &o, const QString &p)
    {
        id(o,p); string(o.value("name"),p + ".name");
        if (!o.value("enabled").isBool()) fail(p + ".enabled",QStringLiteral("Ожидается boolean"));
        integer(o.value("priority"),p + ".priority",-1000000,1000000); condition(o.value("when"),p + ".when");
    }
    void validate(const QJsonObject &o)
    {
        const QString p = QStringLiteral("schedule");
        fields(o,p,{"format","schemaVersion","scheduleId","publicationId","revision","stationId","publishedAt","timeZone","validity",
                    "requiredCapabilities","musicTransition","timeResolution","fallback","assets","playlists","calendars","dayTemplates","baseRules","mixRules","eventRules"},{"$schema"});
        constant(o.value("format"),p + ".format","mediabox.schedule");
        if (o.contains("$schema")) constant(o.value("$schema"),p + ".$schema","urn:mediabox:schedule:v1");
        integer(o.value("schemaVersion"),p + ".schemaVersion",1,1);
        id(o,p,"scheduleId"); id(o,p,"stationId"); id(o,p,"publicationId"); integer(o.value("revision"),p + ".revision",1,2147483647);
        const auto published = string(o.value("publishedAt"),p + ".publishedAt");
        static const QRegularExpression timestamp(QStringLiteral("^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\\.[0-9]+)?Z$"));
        if (!timestamp.match(published).hasMatch() || !QDateTime::fromString(published,Qt::ISODateWithMs).isValid())
            fail(p + ".publishedAt",QStringLiteral("Ожидается реальная дата и время в UTC"));
        date(published.left(10),p + ".publishedAt"); time(published.mid(11,8),p + ".publishedAt");
        zone = QTimeZone(string(o.value("timeZone"),p + ".timeZone").toUtf8());
        if (!zone.isValid()) fail(p + ".timeZone",QStringLiteral("Неизвестный часовой пояс"));
        validity = range(o.value("validity"),p + ".validity");
        constant(o.value("musicTransition"),p + ".musicTransition","finish_track");
        const auto resolution = object(o.value("timeResolution"),p + ".timeResolution");
        fields(resolution,p,{"gap","overlap"}); constant(resolution.value("gap"),p,"skip"); constant(resolution.value("overlap"),p,"first");
        const auto fallback = object(o.value("fallback"),p + ".fallback"); fields(fallback,p,{"source","volumePercent"});
        source(fallback.value("source"),p + ".fallback.source"); integer(fallback.value("volumePercent"),p,0,100);
        for (const auto &value : array(o.value("assets"),"assets")) {
            const auto a = object(value,"assets"); fields(a,"assets",{"id","path","mediaType"}); id(a,"assets");
            const auto mediaType = string(a.value("mediaType"),"assets.mediaType");
            if (mediaType != "audio" && mediaType != "video")
                fail("assets.mediaType",QStringLiteral("Ожидается audio или video"));
            const auto path = string(a.value("path"),"assets.path");
            if (path.startsWith('/') || path.endsWith('/') || path.contains('\\') || path.contains(':')) fail("assets.path",QStringLiteral("Ожидается относительный путь с разделителем /"));
            for (const auto &part : path.split('/'))
                if (part.isEmpty() || part == "." || part == "..") fail("assets.path",QStringLiteral("Пустые сегменты, . и .. запрещены"));
            assets.insert(a.value("id").toString(),a);
        }
        for (const auto &value : array(o.value("playlists"),"playlists")) {
            const auto a = object(value,"playlists"); fields(a,"playlists",{"id","revision","name","order","entries"}); id(a,"playlists");
            integer(a.value("revision"),"playlists.revision",1,2147483647); string(a.value("name"),"playlists.name");
            const auto order = string(a.value("order"),"playlists.order");
            if (order != "sequential" && order != "shuffle_cycle") fail("playlists.order",QStringLiteral("Неизвестный порядок"));
            for (const auto &entry : array(a.value("entries"),"playlists.entries")) {
                const auto e = object(entry,"entries"); fields(e,"entries",{"id","assetId"}); id(e,"entries"); ref(e.value("assetId"),"entries.assetId","asset");
            }
            playlists.insert(a.value("id").toString(),a);
        }
        for (const auto &value : array(o.value("calendars"),"calendars")) {
            const auto a = object(value,"calendars"); fields(a,"calendars",{"id","revision","name","coverage","dates"}); id(a,"calendars");
            integer(a.value("revision"),"calendars.revision",1,2147483647); string(a.value("name"),"calendars.name");
            const auto coverage = range(a.value("coverage"),"calendars.coverage"); dates(a.value("dates"),"calendars.dates");
            for (const auto &d : a.value("dates").toArray())
                if (date(d,"calendars.dates") < coverage.first || date(d,"calendars.dates") >= coverage.second)
                    fail("calendars.dates",QStringLiteral("Дата за пределами coverage"));
            calendars.insert(a.value("id").toString(),a);
        }
        for (const auto &value : array(o.value("dayTemplates"),"dayTemplates")) {
            const auto a = object(value,"dayTemplates"); fields(a,"dayTemplates",{"id","name","slots"}); id(a,"dayTemplates"); string(a.value("name"),"dayTemplates.name");
            QList<QPair<int,int>> windows;
            for (const auto &valueSlot : array(a.value("slots"),"dayTemplates.slots",1)) {
                const auto s = object(valueSlot,"slots"); fields(s,"slots",{"id","window","source","volumePercent"}); id(s,"slots");
                windows.append(window(s.value("window"),"slots.window")); source(s.value("source"),"slots.source"); integer(s.value("volumePercent"),"slots.volumePercent",0,100);
            }
            noWindowOverlap(windows,"dayTemplates.slots"); templates.insert(a.value("id").toString(),a);
        }
        for (const auto &value : array(o.value("baseRules"),"baseRules")) {
            const auto a = object(value,"baseRules"); fields(a,"baseRules",{"id","name","enabled","priority","when","templateId"}); rule(a,"baseRules");
            ref(a.value("templateId"),"baseRules.templateId","template");
        }
        for (const auto &value : array(o.value("mixRules"),"mixRules")) {
            const auto a = object(value,"mixRules"); fields(a,"mixRules",{"id","name","enabled","priority","when","windows","pattern","emptyAdditionalSource"}); rule(a,"mixRules");
            QList<QPair<int,int>> windows;
            for (const auto &w : array(a.value("windows"),"mixRules.windows",1)) windows.append(window(w,"mixRules.windows"));
            noWindowOverlap(windows,"mixRules.windows");
            const auto pattern = array(a.value("pattern"),"mixRules.pattern",2,64); bool additional = false;
            for (const auto &s : pattern) { source(s,"mixRules.pattern",true); additional |= s.toObject().value("type") == "playlist"; }
            if (pattern.first().toObject().value("type") != "active_base" || !additional) fail("mixRules.pattern",QStringLiteral("Первый слот — active_base; требуется дополнительный плейлист"));
            constant(a.value("emptyAdditionalSource"),"mixRules.emptyAdditionalSource","use_base");
        }
        for (const auto &value : array(o.value("eventRules"),"eventRules")) {
            const auto a = object(value,"eventRules"); fields(a,"eventRules",{"id","name","enabled","priority","when","times","action","delivery"}); rule(a,"eventRules");
            QSet<QTime> seen;
            for (const auto &t : array(a.value("times"),"eventRules.times",1)) {
                const auto parsed = time(t,"eventRules.times"); if (seen.contains(parsed)) fail("eventRules.times",QStringLiteral("Повторяющееся время")); seen.insert(parsed);
            }
            const auto action = object(a.value("action"),"eventRules.action"); fields(action,"eventRules.action",{"assetId","volumePercent"});
            ref(action.value("assetId"),"eventRules.action.assetId","asset"); integer(action.value("volumePercent"),"eventRules.action.volumePercent",0,100);
            const auto delivery = object(a.value("delivery"),"eventRules.delivery"); fields(delivery,"eventRules.delivery",{"start","maxLateSeconds","expired","after"});
            const auto start = string(delivery.value("start"),"eventRules.delivery.start");
            if (start != "after_track" && start != "interrupt") fail("eventRules.delivery.start",QStringLiteral("Неподдерживаемый способ запуска"));
            integer(delivery.value("maxLateSeconds"),"eventRules.delivery.maxLateSeconds",0,3600);
            constant(delivery.value("expired"),"eventRules.delivery.expired","skip"); constant(delivery.value("after"),"eventRules.delivery.after","resume_music");
        }
        for (const auto &r : references) {
            const bool found = r.type == "asset" ? assets.contains(r.id) : r.type == "playlist" ? playlists.contains(r.id)
                : r.type == "calendar" ? calendars.contains(r.id) : templates.contains(r.id);
            if (!found) fail(r.path,QStringLiteral("Отсутствует объект %1").arg(r.id));
        }
        for (const auto &id : usedCalendars) {
            const auto coverage = range(calendars.value(id).value("coverage"),"calendars.coverage");
            if (coverage.first > validity.first || coverage.second < validity.second)
                fail("calendars.coverage",QStringLiteral("Календарь %1 не покрывает весь validity").arg(id));
        }
        QSet<QString> capabilities;
        for (const auto &v : array(o.value("requiredCapabilities"),"requiredCapabilities",1)) {
            const auto cap = string(v,"requiredCapabilities");
            if (capabilities.contains(cap)) fail("requiredCapabilities",QStringLiteral("Повторяющаяся возможность"));
            capabilities.insert(cap);
        }
        const auto required = requiredCapabilities(o);
        if (capabilities != QSet<QString>(required.begin(),required.end()))
            fail("requiredCapabilities",QStringLiteral("Возможности должны точно соответствовать правилам: %1").arg(required.join(", ")));
    }
};

bool matches(const QJsonObject &condition, const QDate &d, const Validator &v)
{
    if (!d.isValid()) return false;
    const auto iso = d.toString(Qt::ISODate);
    if (condition.value("excludeDates").toArray().contains(iso)) return false;
    if (condition.contains("range")) {
        const auto r = condition.value("range").toObject();
        if (iso < r.value("from").toString() || iso >= r.value("until").toString()) return false;
    }
    const auto s = condition.value("select").toObject(); const auto type = s.value("type").toString();
    if (type == "all") return true;
    if (type == "weekdays") return s.value("days").toArray().contains(d.dayOfWeek());
    if (type == "dates") return s.value("dates").toArray().contains(iso);
    if (type == "calendar") return v.calendars.value(s.value("calendarId").toString()).value("dates").toArray().contains(iso);
    return s.value("months").toArray().contains(d.month()) && s.value("monthDays").toArray().contains(d.day())
        && s.value("weekdays").toArray().contains(d.dayOfWeek());
}

void checkPreviousCalendar(const QJsonObject &rule, const QJsonArray &windows,
                           const QDate &d, const Validator &v)
{
    if (d >= v.validity.first || !rule.value("enabled").toBool()) return;
    const auto condition = rule.value("when").toObject(), selector = condition.value("select").toObject();
    if (selector.value("type") != "calendar") return;
    const auto iso = d.toString(Qt::ISODate);
    if (condition.value("excludeDates").toArray().contains(iso)) return;
    if (condition.contains("range")) {
        const auto r = condition.value("range").toObject();
        if (iso < r.value("from").toString() || iso >= r.value("until").toString()) return;
    }
    const auto coverage = v.calendars.value(selector.value("calendarId").toString()).value("coverage").toObject();
    if (iso >= coverage.value("from").toString() && iso < coverage.value("until").toString()) return;
    for (const auto &value : windows) {
        const auto w = value.toObject();
        if (w.value("untilDayOffset").toInt() == 1 && w.value("until") != "00:00:00")
            fail("calendars.coverage",QStringLiteral("Календарь не покрывает предыдущую дату %1, необходимую для ночного окна %2")
                 .arg(iso,rule.value("name").toString()));
    }
}

QDateTime localTime(const QDate &d, const QTime &t, const QTimeZone &zone)
{
    // PreferBefore chooses the first occurrence in an overlap. A gap changes
    // the requested wall time; the round-trip check rejects that normalization.
    const QDateTime result(d,t,zone,QDateTime::TransitionResolution::PreferBefore);
    return result.isValid() && result.date() == d && result.time() == t ? result : QDateTime{};
}

void appendWindow(QList<Segment> &segments, Compiled &c, const QDate &d,
                  const QJsonObject &rule, const QJsonObject &slot, const QJsonObject &w)
{
    const auto from = localTime(d,QTime::fromString(w.value("from").toString(),"HH:mm:ss"),c.zone);
    const auto until = localTime(d.addDays(w.value("untilDayOffset").toInt()),QTime::fromString(w.value("until").toString(),"HH:mm:ss"),c.zone);
    if (!from.isValid() || !until.isValid()) {
        c.diagnostics.append({d,QStringLiteral("%1: окно %2 пропущено — граница отсутствует при переводе часов").arg(rule.value("name").toString(),d.toString(Qt::ISODate))});
        return;
    }
    const qint64 begin = std::max(c.from,from.toMSecsSinceEpoch()), end = std::min(c.until,until.toMSecsSinceEpoch());
    if (begin < end) segments.append({begin,end,rule.value("priority").toInt(),rule,slot});
}

QList<Segment> winners(const QList<Segment> &segments, const QString &group, const QTimeZone &zone)
{
    struct Edge { qint64 at; int index; bool start; };
    QList<Edge> edges; edges.reserve(segments.size() * 2);
    for (qsizetype i = 0; i < segments.size(); ++i) {
        edges.append({segments[i].from,int(i),true}); edges.append({segments[i].until,int(i),false});
    }
    std::sort(edges.begin(),edges.end(),[](const Edge &a,const Edge &b) { return a.at < b.at; });
    std::map<int,std::set<int>> active; QList<Segment> result;
    for (qsizetype i = 0; i < edges.size();) {
        const qint64 at = edges[i].at; qsizetype end = i;
        while (end < edges.size() && edges[end].at == at) ++end;
        for (qsizetype j = i; j < end; ++j) if (!edges[j].start) {
            const int priority = segments[edges[j].index].priority; auto it = active.find(priority);
            if (it != active.end()) { it->second.erase(edges[j].index); if (it->second.empty()) active.erase(it); }
        }
        for (qsizetype j = i; j < end; ++j) if (edges[j].start) {
            const auto &s = segments[edges[j].index]; auto &same = active[s.priority];
            if (!same.empty()) {
                const auto &other = segments[*same.begin()];
                const auto ruleName = [](const QJsonObject &rule) {
                    const QString name = rule.value("name").toString().trimmed();
                    return name.isEmpty() ? rule.value("id").toString() : name;
                };
                const QString kind = group == QLatin1String("baseRules")
                        ? QStringLiteral("Базовые правила") : QStringLiteral("Правила чередования");
                const QString from = QDateTime::fromMSecsSinceEpoch(at,zone).toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
                const QString until = QDateTime::fromMSecsSinceEpoch(std::min(other.until,s.until),zone)
                        .toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
                fail(QStringLiteral("Расписание"),QStringLiteral("%1 «%2» и «%3» пересекаются при одинаковом приоритете %4: %5–%6 (%7).")
                     .arg(kind,ruleName(other.rule),ruleName(s.rule)).arg(s.priority)
                     .arg(from,until,QString::fromUtf8(zone.id())));
            }
            same.insert(edges[j].index);
        }
        if (end < edges.size() && !active.empty()) {
            auto winner = segments[*active.rbegin()->second.begin()]; winner.from = at; winner.until = edges[end].at;
            if (!result.isEmpty() && result.last().until == at && result.last().rule.value("id") == winner.rule.value("id")
                && result.last().slot.value("id") == winner.slot.value("id")) result.last().until = winner.until;
            else result.append(winner);
        }
        i = end;
    }
    return result;
}

QSharedPointer<Compiled> collectOccurrences(const QJsonObject &o, const Validator &v,
                                          const QDate &requestedFrom = {}, const QDate &requestedUntil = {})
{
    auto c = QSharedPointer<Compiled>::create(); c->zone = v.zone; c->fromDate = v.validity.first; c->untilDate = v.validity.second;
    c->eventRules = o.value("eventRules").toArray(); c->calendars = v.calendars;
    const auto from = c->fromDate.startOfDay(c->zone), until = c->untilDate.startOfDay(c->zone);
    if (!from.isValid() || !until.isValid()) fail("validity",QStringLiteral("Граница диапазона отсутствует в часовом поясе станции"));
    c->from = from.toMSecsSinceEpoch(); c->until = until.toMSecsSinceEpoch();
    // Previous-date coverage is part of the document contract even if a
    // diagnostic preview asks for a later day in the validity range.
    const QDate previousDate = c->fromDate.addDays(-1);
    for (const auto &value : o.value("baseRules").toArray()) {
        const auto r = value.toObject(); QJsonArray windows;
        for (const auto &slot : v.templates.value(r.value("templateId").toString()).value("slots").toArray())
            windows.append(slot.toObject().value("window"));
        checkPreviousCalendar(r,windows,previousDate,v);
    }
    for (const auto &value : o.value("mixRules").toArray()) {
        const auto r = value.toObject(); checkPreviousCalendar(r,r.value("windows").toArray(),previousDate,v);
    }
    const QDate firstDate = requestedFrom.isValid() ? std::max(previousDate,requestedFrom.addDays(-1)) : previousDate;
    const QDate endDate = requestedUntil.isValid() ? std::min(c->untilDate,requestedUntil) : c->untilDate;
    // Enumerate dates, never minutes. The sweep below only visits boundaries.
    for (QDate d = firstDate; d < endDate; d = d.addDays(1)) {
        for (const auto &value : o.value("baseRules").toArray()) {
            const auto r = value.toObject();
            const auto templateSlots = v.templates.value(r.value("templateId").toString()).value("slots").toArray();
            if (!r.value("enabled").toBool() || !matches(r.value("when").toObject(),d,v)) continue;
            for (const auto &slot : templateSlots)
                appendWindow(c->base,*c,d,r,slot.toObject(),slot.toObject().value("window").toObject());
        }
        for (const auto &value : o.value("mixRules").toArray()) {
            const auto r = value.toObject();
            if (!r.value("enabled").toBool() || !matches(r.value("when").toObject(),d,v)) continue;
            for (const auto &w : r.value("windows").toArray()) appendWindow(c->mix,*c,d,r,{},w.toObject());
        }
        if (d < c->fromDate) continue;
        for (const auto &value : o.value("eventRules").toArray()) {
            const auto r = value.toObject(); if (!r.value("enabled").toBool() || !matches(r.value("when").toObject(),d,v)) continue;
            // Event rules do not conflict. Only DST gaps need publication
            // diagnostics; occurrences are generated for a requested interval.
            // Keeping all future events would make a long validity consume
            // memory proportional to every second-level event in that period.
            if (!c->zone.hasTransitions()) continue;
            const auto dayStart = d.startOfDay(c->zone), dayEnd = d.addDays(1).startOfDay(c->zone);
            if (dayStart.isValid() && dayEnd.isValid() && dayStart.secsTo(dayEnd) == 86400) continue;
            for (const auto &t : r.value("times").toArray()) {
                const auto at = localTime(d,QTime::fromString(t.toString(),"HH:mm:ss"),c->zone);
                if (!at.isValid()) {
                    c->diagnostics.append({d,QStringLiteral("%1: событие %2 %3 пропущено — локальное время отсутствует")
                        .arg(r.value("name").toString(),d.toString(Qt::ISODate),t.toString())}); continue;
                }
            }
        }
    }
    return c;
}

QSharedPointer<Compiled> compile(const QJsonObject &o, const Validator &v)
{
    auto c = collectOccurrences(o,v);
    c->base = winners(c->base,"baseRules",c->zone); c->mix = winners(c->mix,"mixRules",c->zone);
    return c;
}

QList<RuleConflict> conflicts(const QList<Segment> &segments, const QString &group, const QTimeZone &zone)
{
    struct Edge { qint64 at; int index; bool start; };
    QList<Edge> edges;
    for (qsizetype i = 0; i < segments.size(); ++i) {
        edges.append({segments[i].from,int(i),true}); edges.append({segments[i].until,int(i),false});
    }
    std::sort(edges.begin(),edges.end(),[](const Edge &a,const Edge &b) { return a.at < b.at; });
    std::map<int,std::set<int>> active;
    QList<RuleConflict> result;
    for (qsizetype i = 0; i < edges.size();) {
        const qint64 at = edges[i].at; qsizetype end = i;
        while (end < edges.size() && edges[end].at == at) ++end;
        for (qsizetype j = i; j < end; ++j) if (!edges[j].start) {
            auto it = active.find(segments[edges[j].index].priority);
            if (it != active.end()) { it->second.erase(edges[j].index); if (it->second.empty()) active.erase(it); }
        }
        for (qsizetype j = i; j < end; ++j) if (edges[j].start)
            active[segments[edges[j].index].priority].insert(edges[j].index);
        if (end < edges.size()) for (const auto &[priority,indexes] : active) {
            if (indexes.size() < 2) continue;
            QStringList ids;
            for (const int index : indexes) ids.append(segments[index].rule.value("id").toString());
            ids.removeDuplicates(); ids.sort();
            const auto from = QDateTime::fromMSecsSinceEpoch(at,zone);
            const auto until = QDateTime::fromMSecsSinceEpoch(edges[end].at,zone);
            if (!result.isEmpty() && result.last().until == from && result.last().priority == priority
                    && result.last().ruleIds == ids) result.last().until = until;
            else result.append({from,until,group,ids,priority});
        }
        i = end;
    }
    return result;
}

const Segment *atSegment(const QList<Segment> &segments, qint64 at)
{
    const auto next = std::upper_bound(segments.begin(),segments.end(),at,[](qint64 time,const Segment &s) { return time < s.from; });
    if (next == segments.begin()) return nullptr;
    const auto &s = *std::prev(next); return at < s.until ? &s : nullptr;
}
}

QString strictJsonObject(const QByteArray &bytes, QJsonObject *result)
{
    try {
        if (!result) fail("JSON",QStringLiteral("Не задан получатель"));
        if (bytes.startsWith("\xef\xbb\xbf")) fail("JSON",QStringLiteral("UTF-8 BOM запрещён"));
        QStringDecoder decoder(QStringDecoder::Utf8,QStringConverter::Flag::Stateless);
        const QString decoded = decoder.decode(bytes); Q_UNUSED(decoded);
        if (decoder.hasError()) fail("JSON",QStringLiteral("Некорректный UTF-8"));
        QJsonParseError error; const auto parsed = QJsonDocument::fromJson(bytes,&error);
        if (error.error != QJsonParseError::NoError || !parsed.isObject()) fail("JSON",QStringLiteral("%1; требуется объект").arg(error.errorString()));
        JsonKeys(bytes).check(); *result = parsed.object(); return {};
    } catch (const Invalid &error) { return error.text; }
}

QStringList requiredCapabilities(const QJsonObject &o)
{
    QStringList result{QStringLiteral("calendar.v1")};
    if (!o.value("mixRules").toArray().isEmpty()) result.append(QStringLiteral("rotation.strict.v1"));
    if (!o.value("eventRules").toArray().isEmpty()) result.append(QStringLiteral("events.fixed.v1"));
    for (const auto &asset : o.value("assets").toArray()) {
        if (asset.toObject().value("mediaType") == QJsonValue("video")) {
            result.append(QStringLiteral("media.video.v1"));
            break;
        }
    }
    return result;
}

QString parse(const QByteArray &bytes, Document *result)
{
    QJsonObject object; const auto error = strictJsonObject(bytes,&object);
    return error.isEmpty() ? decode(object,result) : error;
}

QString decode(const QJsonObject &object, Document *result)
{
    try {
        if (!result) fail("schedule",QStringLiteral("Не задан получатель"));
        Validator validator; validator.validate(object);
        Document replacement; replacement.object = object; replacement.compiled = compile(object,validator);
        for (const auto &diagnostic : replacement.compiled->diagnostics) replacement.diagnostics.append(diagnostic.text);
        *result = replacement; return {};
    } catch (const Invalid &error) { return error.text; }
}

Evaluation evaluate(const Document &document, const QDateTime &at)
{
    Evaluation result;
    const auto fallback = document.object.value("fallback").toObject();
    auto source = fallback.value("source").toObject(); result.volumePercent = fallback.value("volumePercent").toInt(100);
    if (!document.compiled || !at.isValid()) {
        result.diagnostics.append(QStringLiteral("Расписание или момент времени не проверены")); return result;
    }
    const auto &c = *document.compiled; const qint64 now = at.toMSecsSinceEpoch();
    result.withinValidity = now >= c.from && now < c.until;
    if (result.withinValidity) {
        if (const auto *base = atSegment(c.base,now)) {
            result.usingFallback = false; result.baseRuleId = base->rule.value("id").toString(); result.baseSlotId = base->slot.value("id").toString();
            source = base->slot.value("source").toObject(); result.volumePercent = base->slot.value("volumePercent").toInt();
        }
        if (const auto *mix = atSegment(c.mix,now)) {
            result.mixRuleId = mix->rule.value("id").toString(); result.pattern = mix->rule.value("pattern").toArray();
            result.activationStart = QDateTime::fromMSecsSinceEpoch(mix->from,QTimeZone::UTC);
        }
        const auto localDate = at.toTimeZone(c.zone).date();
        for (const auto &d : c.diagnostics) if (d.date == localDate || d.date == localDate.addDays(-1)) result.diagnostics.append(d.text);
    } else result.diagnostics.append(QStringLiteral("Расписание вне validity; применяется резервный источник"));
    result.silence = source.value("type") == "silence";
    result.playlistId = source.value("playlistId").toString();
    return result;
}

QList<PlanInterval> intervals(const Document &document, const QDateTime &fromInclusive, const QDateTime &untilExclusive)
{
    QList<PlanInterval> result;
    if (!document.compiled || !fromInclusive.isValid() || !untilExclusive.isValid() || fromInclusive >= untilExclusive)
        return result;
    const auto &c = *document.compiled;
    const qint64 from = fromInclusive.toMSecsSinceEpoch(), until = untilExclusive.toMSecsSinceEpoch();
    QList<qint64> boundaries{from,until};
    const auto addBoundary = [&](qint64 boundary) {
        if (boundary > from && boundary < until) boundaries.append(boundary);
    };
    addBoundary(c.from); addBoundary(c.until);
    const auto addSegments = [&](const QList<Segment> &segments) {
        auto segment = std::upper_bound(segments.cbegin(),segments.cend(),from,
                [](qint64 at,const Segment &s) { return at < s.until; });
        for (; segment != segments.cend() && segment->from < until; ++segment) {
            addBoundary(segment->from); addBoundary(segment->until);
        }
    };
    addSegments(c.base); addSegments(c.mix);
    // evaluate() reports skipped DST occurrences on their date and the next
    // local day. Retain those changes even if the audible plan stays the same.
    for (const auto &diagnostic : c.diagnostics) {
        for (const auto &date : {diagnostic.date,diagnostic.date.addDays(2)}) {
            const auto boundary = date.startOfDay(c.zone);
            if (boundary.isValid() && boundary.toMSecsSinceEpoch() > c.from && boundary.toMSecsSinceEpoch() < c.until)
                addBoundary(boundary.toMSecsSinceEpoch());
        }
    }
    std::sort(boundaries.begin(),boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
    const auto samePlan = [](const Evaluation &left,const Evaluation &right) {
        return left.withinValidity == right.withinValidity && left.usingFallback == right.usingFallback
                && left.baseRuleId == right.baseRuleId && left.baseSlotId == right.baseSlotId
                && left.playlistId == right.playlistId && left.silence == right.silence
                && left.volumePercent == right.volumePercent && left.mixRuleId == right.mixRuleId
                && left.pattern == right.pattern && left.activationStart == right.activationStart
                && left.diagnostics == right.diagnostics;
    };
    for (qsizetype i = 0; i + 1 < boundaries.size(); ++i) {
        const auto begin = QDateTime::fromMSecsSinceEpoch(boundaries[i],c.zone);
        const auto end = QDateTime::fromMSecsSinceEpoch(boundaries[i + 1],c.zone);
        const auto plan = evaluate(document,begin);
        if (!result.isEmpty() && samePlan(result.last().plan,plan)) result.last().until = end;
        else result.append({begin,end,plan});
    }
    return result;
}

QList<EventOccurrence> events(const Document &document, const QDateTime &fromInclusive, const QDateTime &toInclusive)
{
    QList<EventOccurrence> result;
    if (!document.compiled || !fromInclusive.isValid() || !toInclusive.isValid() || fromInclusive > toInclusive) return result;
    const auto &c = *document.compiled;
    const QDate first = std::max(c.fromDate,fromInclusive.toTimeZone(c.zone).date());
    const QDate last = std::min(c.untilDate.addDays(-1),toInclusive.toTimeZone(c.zone).date());
    Validator context; context.calendars = c.calendars;
    for (QDate d = first; d <= last; d = d.addDays(1)) {
        for (const auto &value : c.eventRules) {
            const auto r = value.toObject();
            if (!r.value("enabled").toBool() || !matches(r.value("when").toObject(),d,context)) continue;
            const auto action = r.value("action").toObject(), delivery = r.value("delivery").toObject();
            for (const auto &t : r.value("times").toArray()) {
                const auto scheduled = localTime(d,QTime::fromString(t.toString(),"HH:mm:ss"),c.zone);
                if (!scheduled.isValid() || scheduled < fromInclusive || scheduled > toInclusive) continue;
                result.append({r.value("id").toString(),action.value("assetId").toString(),scheduled.toUTC(),
                    r.value("priority").toInt(),action.value("volumePercent").toInt(),
                    delivery.value("maxLateSeconds").toInt(),delivery.value("start").toString()});
            }
        }
    }
    std::sort(result.begin(),result.end(),[](const EventOccurrence &a,const EventOccurrence &b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        if (a.scheduledUtc != b.scheduledUtc) return a.scheduledUtc < b.scheduledUtc;
        return a.ruleId < b.ruleId;
    });
    return result;
}

DiagnosticPreview diagnosticPreview(const QJsonObject &object, const QDateTime &fromInclusive,
                                    const QDateTime &untilExclusive)
{
    try {
        if (!fromInclusive.isValid() || !untilExclusive.isValid() || fromInclusive >= untilExclusive)
            fail("preview",QStringLiteral("Некорректный диапазон предпросмотра"));
        Validator validator; validator.validate(object);
        auto occurrences = collectOccurrences(object,validator,fromInclusive.toTimeZone(validator.zone).date(),
                untilExclusive.addMSecs(-1).toTimeZone(validator.zone).date().addDays(1));
        const qint64 from = fromInclusive.toMSecsSinceEpoch(), until = untilExclusive.toMSecsSinceEpoch();
        const auto clip = [&](QList<Segment> &segments) {
            QList<Segment> visible;
            for (auto segment : segments) {
                segment.from = std::max(from,segment.from); segment.until = std::min(until,segment.until);
                if (segment.from < segment.until) visible.append(std::move(segment));
            }
            std::stable_sort(visible.begin(),visible.end(),[](const Segment &a,const Segment &b) { return a.from < b.from; });
            segments = std::move(visible);
        };
        clip(occurrences->base); clip(occurrences->mix);
        DiagnosticPreview result;
        const auto append = [&](const QList<Segment> &segments, QList<PlanInterval> &destination, bool mix) {
            for (const auto &segment : segments) {
                Evaluation plan; plan.withinValidity = true; plan.usingFallback = false;
                if (mix) {
                    plan.mixRuleId = segment.rule.value("id").toString();
                    plan.pattern = segment.rule.value("pattern").toArray();
                } else {
                    const auto source = segment.slot.value("source").toObject();
                    plan.baseRuleId = segment.rule.value("id").toString();
                    plan.baseSlotId = segment.slot.value("id").toString();
                    plan.playlistId = source.value("playlistId").toString();
                    plan.silence = source.value("type") == QJsonValue("silence");
                    plan.volumePercent = segment.slot.value("volumePercent").toInt();
                }
                destination.append({QDateTime::fromMSecsSinceEpoch(segment.from,validator.zone),
                    QDateTime::fromMSecsSinceEpoch(segment.until,validator.zone),plan});
            }
        };
        append(occurrences->base,result.intervals,false);
        append(occurrences->mix,result.mixIntervals,true);
        result.conflicts = conflicts(occurrences->base,QStringLiteral("baseRules"),validator.zone)
                + conflicts(occurrences->mix,QStringLiteral("mixRules"),validator.zone);
        for (const auto &diagnostic : occurrences->diagnostics) result.diagnostics.append(diagnostic.text);
        // Only the event enumerator sees this local container. The raw rule
        // occurrences never escape as a Document that evaluate() could execute.
        Document eventContext; eventContext.object = object; eventContext.compiled = occurrences;
        result.events = events(eventContext,fromInclusive,untilExclusive.addMSecs(-1));
        return result;
    } catch (const Invalid &error) {
        DiagnosticPreview result; result.error = error.text; return result;
    }
}
} // namespace ScheduleV1
