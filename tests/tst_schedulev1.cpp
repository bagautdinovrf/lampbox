#include "schedulecore/schedulev1.h"

#include <QFile>
#include <QJsonDocument>
#include <QTest>
#include <QTimeZone>

using namespace ScheduleV1;

namespace {
QString id(int n) { return QStringLiteral("00000000-0000-4000-8000-%1").arg(n,12,10,QChar('0')); }
QJsonObject window(const QString &from = "00:00:00", const QString &until = "00:00:00", int offset = 1)
{ return {{"from",from},{"until",until},{"untilDayOffset",offset}}; }
QJsonObject when(const QJsonObject &select = {{"type","all"}})
{ return {{"select",select},{"excludeDates",QJsonArray{}}}; }
QJsonObject source(int playlist = 11)
{ return {{"type","playlist"},{"playlistId",id(playlist)}}; }
QJsonObject slot(int n, const QJsonObject &w = window(), const QJsonObject &s = source())
{ return {{"id",id(n)},{"window",w},{"source",s},{"volumePercent",65}}; }
QJsonObject base(int n = 30, int templ = 20, int priority = 10, const QJsonObject &condition = when())
{ return {{"id",id(n)},{"name",QString::number(n)},{"enabled",true},{"priority",priority},{"when",condition},{"templateId",id(templ)}}; }
QJsonObject mix(int n = 40, const QJsonArray &windows = {window()}, int priority = 10)
{
    return {{"id",id(n)},{"name",QString::number(n)},{"enabled",true},{"priority",priority},{"when",when()},
            {"windows",windows},{"pattern",QJsonArray{QJsonObject{{"type","active_base"}},source(12)}},{"emptyAdditionalSource","use_base"}};
}
QJsonObject eventRule(int n, const QJsonArray &times, int priority = 0)
{
    return {{"id",id(n)},{"name",QString::number(n)},{"enabled",true},{"priority",priority},{"when",when()},{"times",times},
            {"action",QJsonObject{{"assetId",id(10)},{"volumePercent",80}}},
            {"delivery",QJsonObject{{"start","interrupt"},{"maxLateSeconds",60},{"expired","skip"},{"after","resume_music"}}}};
}
QJsonObject fixture()
{
    return {{"format","mediabox.schedule"},{"schemaVersion",1},{"scheduleId",id(1)},{"stationId",id(2)},{"publicationId",id(3)},
            {"revision",1},{"publishedAt","2026-10-03T09:00:00Z"},{"timeZone","Europe/Moscow"},
            {"validity",QJsonObject{{"from","2026-10-04"},{"until","2026-10-07"}}},
            {"requiredCapabilities",QJsonArray{"calendar.v1"}},{"musicTransition","finish_track"},
            {"timeResolution",QJsonObject{{"gap","skip"},{"overlap","first"}}},
            {"fallback",QJsonObject{{"source",QJsonObject{{"type","silence"}}},{"volumePercent",0}}},
            {"assets",QJsonArray{QJsonObject{{"id",id(10)},{"path","music/Песня.wav"},{"mediaType","audio"}}}},
            {"playlists",QJsonArray{
                QJsonObject{{"id",id(11)},{"revision",1},{"name","База"},{"order","sequential"},{"entries",QJsonArray{QJsonObject{{"id",id(13)},{"assetId",id(10)}}}}},
                QJsonObject{{"id",id(12)},{"revision",1},{"name","Праздник"},{"order","shuffle_cycle"},{"entries",QJsonArray{QJsonObject{{"id",id(14)},{"assetId",id(10)}}}}}}},
            {"calendars",QJsonArray{}},{"dayTemplates",QJsonArray{QJsonObject{{"id",id(20)},{"name","День"},{"slots",QJsonArray{slot(21)}}}}},
            {"baseRules",QJsonArray{base()}},{"mixRules",QJsonArray{}},{"eventRules",QJsonArray{}}};
}
QDateTime at(const QString &iso) { return QDateTime::fromString(iso,Qt::ISODate); }
QString read(QJsonObject o, Document *d)
{
    QJsonArray caps;
    for (const auto &cap : requiredCapabilities(o)) caps.append(cap);
    o.insert("requiredCapabilities",caps);
    return parse(QJsonDocument(o).toJson(QJsonDocument::Compact),d);
}
void replaceWindow(QJsonObject &o, const QJsonObject &w)
{
    auto templates = o.value("dayTemplates").toArray(); auto t = templates[0].toObject();
    t.insert("slots",QJsonArray{slot(21,w)}); templates[0] = t; o.insert("dayTemplates",templates);
}
}

class ScheduleV1Tests : public QObject {
    Q_OBJECT
private slots:
    void sharedAudioVideoContractAndStrictVersion()
    {
        auto object = fixture();
        auto assets = object.value("assets").toArray();
        auto asset = assets.first().toObject();
        asset.insert("mediaType", "video");
        asset.insert("path", "video/Экран/ролик.mp4");
        assets[0] = asset;
        object.insert("assets", assets);
        Document document;
        QVERIFY2(read(object, &document).isEmpty(), qPrintable(read(object, &document)));
        QVERIFY(requiredCapabilities(object).contains(QStringLiteral("media.video.v1")));
        QCOMPARE(evaluate(document, at("2026-10-04T12:00:00+03:00")).playlistId, id(11));
        object.insert("schemaVersion", 0);
        QVERIFY(!read(object, &document).isEmpty());
        object.insert("schemaVersion", 2);
        QVERIFY(!read(object, &document).isEmpty());
        object.insert("schemaVersion", 1);
        asset.insert("mediaType", "unknown");
        assets[0] = asset;
        object.insert("assets", assets);
        QVERIFY(!read(object, &document).isEmpty());
    }
    void documentedExample()
    {
        QFile file(QFINDTESTDATA("../Documentation/schedule-v1/example.new-year.json"));
        QVERIFY(file.open(QIODevice::ReadOnly)); Document document;
        const auto error = parse(file.readAll(),&document); QVERIFY2(error.isEmpty(),qPrintable(error));
        QCOMPARE(document.revision(),7);
    }
    void fullDayIsExplicitAndHalfOpen()
    {
        Document d; auto o = fixture(); QVERIFY(read(o,&d).isEmpty());
        QVERIFY(evaluate(d,at("2026-10-03T23:59:59+03:00")).silence);
        QCOMPARE(evaluate(d,at("2026-10-04T00:00:00+03:00")).playlistId,id(11));
        QCOMPARE(evaluate(d,at("2026-10-06T23:59:59.999+03:00")).playlistId,id(11));
        const auto expired = evaluate(d,at("2026-10-07T00:00:00+03:00"));
        QVERIFY(!expired.withinValidity); QVERIFY(expired.silence); QVERIFY(expired.usingFallback);
        replaceWindow(o,window("00:00:00","00:00:00",0)); QVERIFY(!read(o,&d).isEmpty());
        replaceWindow(o,window("08:00:00","09:00:00",1)); QVERIFY(!read(o,&d).isEmpty());
    }
    void overnightUsesStartDateAndClipsValidity()
    {
        auto o = fixture(); replaceWindow(o,window("22:00:00","02:00:00",1));
        o.insert("baseRules",QJsonArray{base(30,20,10,when({{"type","weekdays"},{"days",QJsonArray{6}}}))});
        Document d; QVERIFY(read(o,&d).isEmpty());
        QCOMPARE(evaluate(d,at("2026-10-04T01:59:59+03:00")).playlistId,id(11));
        QVERIFY(evaluate(d,at("2026-10-04T02:00:00+03:00")).silence);
        QVERIFY(evaluate(d,at("2026-10-04T22:30:00+03:00")).silence);
    }
    void calendarSelectorsAndExclusions()
    {
        const QList<QJsonObject> selectors{
            {{"type","all"}}, {{"type","weekdays"},{"days",QJsonArray{7}}},
            {{"type","dates"},{"dates",QJsonArray{"2026-10-04"}}},
            {{"type","annual_filter"},{"months",QJsonArray{10}},{"monthDays",QJsonArray{4}},{"weekdays",QJsonArray{7}}},
            {{"type","calendar"},{"calendarId",id(50)}}};
        for (const auto &selector : selectors) {
            auto o = fixture(); auto condition = when(selector);
            condition.insert("range",QJsonObject{{"from","2026-10-04"},{"until","2026-10-05"}});
            o.insert("baseRules",QJsonArray{base(30,20,10,condition)});
            o.insert("calendars",QJsonArray{QJsonObject{{"id",id(50)},{"revision",1},{"name","Календарь"},
                {"coverage",o.value("validity")},{"dates",QJsonArray{"2026-10-04"}}}});
            Document d; const auto error = read(o,&d); QVERIFY2(error.isEmpty(),qPrintable(error));
            QVERIFY(!evaluate(d,at("2026-10-04T12:00:00+03:00")).silence);
            QVERIFY(evaluate(d,at("2026-10-05T12:00:00+03:00")).silence);
            condition.insert("excludeDates",QJsonArray{"2026-10-04"}); o.insert("baseRules",QJsonArray{base(30,20,10,condition)});
            QVERIFY(read(o,&d).isEmpty()); QVERIFY(evaluate(d,at("2026-10-04T12:00:00+03:00")).silence);
        }
    }
    void priorityOnlyAppliesInsideSlotsAndSilenceWins()
    {
        auto o = fixture(); auto templates = o.value("dayTemplates").toArray();
        templates.append(QJsonObject{{"id",id(22)},{"name","Тишина"},{"slots",QJsonArray{slot(23,window("12:00:00","13:00:00",0),{{"type","silence"}})}}});
        o.insert("dayTemplates",templates); o.insert("baseRules",QJsonArray{base(),base(31,22,20)});
        o.insert("mixRules",QJsonArray{mix()});
        Document d; QVERIFY(read(o,&d).isEmpty());
        QVERIFY(!evaluate(d,at("2026-10-04T11:59:59+03:00")).silence);
        const auto quiet = evaluate(d,at("2026-10-04T12:00:00+03:00"));
        QVERIFY(quiet.silence); QVERIFY(!quiet.usingFallback); QCOMPARE(quiet.baseRuleId,id(31)); QCOMPARE(quiet.mixRuleId,id(40));
        QCOMPARE(evaluate(d,at("2026-10-04T13:00:00+03:00")).baseRuleId,id(30));
        o.insert("baseRules",QJsonArray{base(),base(31,22,10)});
        QVERIFY(read(o,&d).contains(QStringLiteral("одинаковом приоритете")));
    }
    void conflictValidationCoversFutureAndPreviousDay()
    {
        auto o = fixture(); o.insert("baseRules",QJsonArray{base(),base(31,20,10,when({{"type","dates"},{"dates",QJsonArray{"2026-10-06"}}}))});
        Document d; QVERIFY(!read(o,&d).isEmpty());
        replaceWindow(o,window("23:00:00","02:00:00",1));
        o.insert("baseRules",QJsonArray{base(30,20,10,when({{"type","dates"},{"dates",QJsonArray{"2026-10-03"}}})),
                                        base(31,20,10,when({{"type","dates"},{"dates",QJsonArray{"2026-10-03"}}}))});
        QVERIFY(!read(o,&d).isEmpty());
    }
    void rotationActivationSurvivesMidnightAndRestartsAfterWinnerChanges()
    {
        auto o = fixture(); o.insert("mixRules",QJsonArray{mix()}); Document d; QVERIFY(read(o,&d).isEmpty());
        const auto first = evaluate(d,at("2026-10-04T23:59:59+03:00"));
        QCOMPARE(evaluate(d,at("2026-10-05T00:00:00+03:00")).activationStart,first.activationStart);
        o.insert("mixRules",QJsonArray{mix(),mix(41,{window("12:00:00","13:00:00",0)},20)});
        QVERIFY(read(o,&d).isEmpty());
        QCOMPARE(evaluate(d,at("2026-10-05T12:00:00+03:00")).mixRuleId,id(41));
        const auto returned = evaluate(d,at("2026-10-05T13:00:00+03:00"));
        QCOMPARE(returned.mixRuleId,id(40)); QCOMPARE(returned.activationStart,at("2026-10-05T10:00:00Z"));
    }
    void intervalsCoverFallbackGapsAndValidity()
    {
        auto o = fixture();
        o.insert("validity",QJsonObject{{"from","2026-10-04"},{"until","2026-10-05"}});
        replaceWindow(o,window("08:00:01","09:00:02",0));
        Document d; QVERIFY(read(o,&d).isEmpty());
        const auto from = at("2026-10-03T23:00:00+03:00"), until = at("2026-10-05T01:00:00+03:00");
        const auto list = intervals(d,from,until);
        QCOMPARE(list.size(),5);
        const QList<QDateTime> bounds{from,at("2026-10-04T00:00:00+03:00"),at("2026-10-04T08:00:01+03:00"),
                at("2026-10-04T09:00:02+03:00"),at("2026-10-05T00:00:00+03:00"),until};
        for (qsizetype i = 0; i < list.size(); ++i) {
            QCOMPARE(list[i].from,bounds[i]); QCOMPARE(list[i].until,bounds[i + 1]);
            QCOMPARE(list[i].from.timeZone().id(),QByteArray("Europe/Moscow"));
            QCOMPARE(list[i].plan.withinValidity,i > 0 && i < 4);
            QCOMPARE(list[i].plan.usingFallback,i != 2);
            QCOMPARE(list[i].plan.silence,i != 2);
        }
        QCOMPARE(list[2].plan.playlistId,id(11));
        QCOMPARE(list[2].plan.volumePercent,65);
        QVERIFY(intervals(Document{},from,until).isEmpty());
        QVERIFY(intervals(d,QDateTime{},until).isEmpty());
        QVERIFY(intervals(d,from,QDateTime{}).isEmpty());
        QVERIFY(intervals(d,from,from).isEmpty());
        QVERIFY(intervals(d,until,from).isEmpty());
        const auto clipped = intervals(d,bounds[2].addMSecs(250),bounds[3]);
        QCOMPARE(clipped.size(),1);
        QCOMPARE(clipped.first().from,bounds[2].addMSecs(250));
        QVERIFY(!clipped.first().plan.usingFallback);
    }
    void intervalsIncludeOvernightTailFromPreviousStartDate()
    {
        auto o = fixture(); replaceWindow(o,window("22:00:00","02:00:00",1));
        o.insert("baseRules",QJsonArray{base(30,20,10,when({{"type","weekdays"},{"days",QJsonArray{6}}}))});
        Document d; QVERIFY(read(o,&d).isEmpty());
        const auto list = intervals(d,at("2026-10-04T00:00:00+03:00"),at("2026-10-05T00:00:00+03:00"));
        QCOMPARE(list.size(),2);
        QCOMPARE(list[0].from,at("2026-10-04T00:00:00+03:00"));
        QCOMPARE(list[0].until,at("2026-10-04T02:00:00+03:00"));
        QCOMPARE(list[0].plan.baseRuleId,id(30));
        QCOMPARE(list[1].from,list[0].until);
        QCOMPARE(list[1].until,at("2026-10-05T00:00:00+03:00"));
        QVERIFY(list[1].plan.usingFallback);
    }
    void intervalsPreserveWinningBaseAndMixSwitchesToTheSecond()
    {
        auto o = fixture();
        o.insert("dayTemplates",QJsonArray{
            QJsonObject{{"id",id(20)},{"name","День"},{"slots",QJsonArray{
                slot(21,window("00:00:00","12:00:00",0)),slot(24,window("12:00:00","00:00:00",1))}}},
            QJsonObject{{"id",id(22)},{"name","Приоритет"},{"slots",QJsonArray{
                slot(23,window("11:59:58","12:00:02",0),source(12))}}}});
        o.insert("baseRules",QJsonArray{base(),base(31,22,20)});
        o.insert("mixRules",QJsonArray{mix(),mix(41,{window("12:00:01","12:00:03",0)},20)});
        Document d; QVERIFY(read(o,&d).isEmpty());
        const auto list = intervals(d,at("2026-10-04T11:59:57+03:00"),at("2026-10-04T12:00:04+03:00"));
        QCOMPARE(list.size(),5);
        const QList<QDateTime> bounds{at("2026-10-04T11:59:57+03:00"),at("2026-10-04T11:59:58+03:00"),
            at("2026-10-04T12:00:01+03:00"),at("2026-10-04T12:00:02+03:00"),
            at("2026-10-04T12:00:03+03:00"),at("2026-10-04T12:00:04+03:00")};
        const QList<int> rules{30,31,31,30,30}, slotIds{21,23,23,24,24}, mixes{40,40,41,41,40};
        for (qsizetype i = 0; i < list.size(); ++i) {
            QCOMPARE(list[i].from,bounds[i]); QCOMPARE(list[i].until,bounds[i + 1]);
            QCOMPARE(list[i].plan.baseRuleId,id(rules[i]));
            QCOMPARE(list[i].plan.baseSlotId,id(slotIds[i]));
            QCOMPARE(list[i].plan.mixRuleId,id(mixes[i]));
        }
        QCOMPARE(list[1].plan.playlistId,id(12));
        QCOMPARE(list[0].plan.activationStart,at("2026-10-04T00:00:00+03:00"));
        QCOMPARE(list[1].plan.activationStart,list[0].plan.activationStart);
        QCOMPARE(list[4].plan.activationStart,bounds[4]);
        QVERIFY(list[4].plan.activationStart != list[0].plan.activationStart);
    }
    void intervalsKeepSlotIdentityAndMergeContinuousDays()
    {
        auto o = fixture(); Document d; QVERIFY(read(o,&d).isEmpty());
        const auto from = at("2026-10-04T00:00:00+03:00"), until = at("2026-10-06T00:00:00+03:00");
        const auto continuous = intervals(d,from,until);
        QCOMPARE(continuous.size(),1);
        QCOMPARE(continuous.first().from,from); QCOMPARE(continuous.first().until,until);
        o.insert("dayTemplates",QJsonArray{QJsonObject{{"id",id(20)},{"name","День"},{"slots",QJsonArray{
            slot(21,window("00:00:00","12:00:00",0)),slot(24,window("12:00:00","00:00:00",1))}}}});
        QVERIFY(read(o,&d).isEmpty());
        const auto daySlots = intervals(d,at("2026-10-04T11:00:00+03:00"),at("2026-10-04T13:00:00+03:00"));
        QCOMPARE(daySlots.size(),2);
        QCOMPARE(daySlots[0].until,at("2026-10-04T12:00:00+03:00"));
        QCOMPARE(daySlots[0].plan.playlistId,daySlots[1].plan.playlistId);
        QCOMPARE(daySlots[0].plan.baseSlotId,id(21)); QCOMPARE(daySlots[1].plan.baseSlotId,id(24));
    }
    void intervalsRespectDstElapsedTimeAndFirstOverlap()
    {
        auto o = fixture(); o.insert("timeZone","Europe/Berlin");
        o.insert("validity",QJsonObject{{"from","2026-03-29"},{"until","2026-03-30"}});
        replaceWindow(o,window("01:59:59","03:00:01",0));
        Document d; QVERIFY(read(o,&d).isEmpty());
        auto list = intervals(d,at("2026-03-29T00:59:58Z"),at("2026-03-29T01:00:02Z"));
        QCOMPARE(list.size(),3);
        QCOMPARE(list[1].from,at("2026-03-29T00:59:59Z"));
        QCOMPARE(list[1].until,at("2026-03-29T01:00:01Z"));
        QCOMPARE(list[1].from.secsTo(list[1].until),2);
        QVERIFY(!list[1].plan.usingFallback);
        o.insert("validity",QJsonObject{{"from","2026-10-25"},{"until","2026-10-26"}});
        replaceWindow(o,window("02:30:00","03:00:00",0));
        QVERIFY(read(o,&d).isEmpty());
        list = intervals(d,at("2026-10-25T00:00:00Z"),at("2026-10-25T02:30:00Z"));
        QCOMPARE(list.size(),3);
        QCOMPARE(list[1].from,at("2026-10-25T00:30:00Z"));
        QCOMPARE(list[1].until,at("2026-10-25T02:00:00Z"));
        QCOMPARE(list[1].from.secsTo(list[1].until),90 * 60);
    }
    void dstGapSkipsAndOverlapUsesFirstOccurrence()
    {
        auto o = fixture(); o.insert("timeZone","Europe/Berlin");
        o.insert("validity",QJsonObject{{"from","2026-03-29"},{"until","2026-03-30"}});
        o.insert("eventRules",QJsonArray{eventRule(60,{"02:30:00","03:30:00"})});
        Document d; QVERIFY(read(o,&d).isEmpty());
        const auto spring = events(d,at("2026-03-28T00:00:00Z"),at("2026-03-30T00:00:00Z"));
        QCOMPARE(spring.size(),1); QCOMPARE(spring.first().scheduledUtc,at("2026-03-29T01:30:00Z"));
        QVERIFY(!evaluate(d,at("2026-03-29T12:00:00Z")).diagnostics.isEmpty());
        replaceWindow(o,window("02:30:00","04:00:00",0)); QVERIFY(read(o,&d).isEmpty());
        QVERIFY(evaluate(d,at("2026-03-29T01:45:00Z")).silence);
        o.insert("validity",QJsonObject{{"from","2026-10-25"},{"until","2026-10-26"}});
        o.insert("eventRules",QJsonArray{eventRule(60,{"02:30:00"})}); QVERIFY(read(o,&d).isEmpty());
        const auto autumn = events(d,at("2026-10-24T00:00:00Z"),at("2026-10-26T00:00:00Z"));
        QCOMPARE(autumn.size(),1); QCOMPARE(autumn.first().scheduledUtc,at("2026-10-25T00:30:00Z"));
    }
    void eventsHaveStablePriorityOrderAndUtcBounds()
    {
        auto o = fixture(); o.insert("eventRules",QJsonArray{eventRule(62,{"12:00:00"},5),eventRule(61,{"12:00:00"},5),eventRule(60,{"12:01:00"},10)});
        Document d; QVERIFY(read(o,&d).isEmpty());
        const auto list = events(d,at("2026-10-04T09:00:00Z"),at("2026-10-04T09:01:00Z"));
        QCOMPARE(list.size(),3); QCOMPARE(list[0].ruleId,id(60)); QCOMPARE(list[1].ruleId,id(61)); QCOMPARE(list[2].ruleId,id(62));
        QCOMPARE(list[0].start,QStringLiteral("interrupt")); QCOMPARE(list[0].maxLateSeconds,60);
        QVERIFY(events(d,at("2026-10-07T00:00:00Z"),at("2026-10-08T00:00:00Z")).isEmpty());
    }
    void strictInputAndAtomicReplacement()
    {
        Document d; QVERIFY(read(fixture(),&d).isEmpty()); const auto prior = d.publicationId();
        auto bytes = QJsonDocument(fixture()).toJson(QJsonDocument::Compact);
        QVERIFY(!parse(QByteArray("\xef\xbb\xbf") + bytes,&d).isEmpty());
        QVERIFY(!parse(QByteArray("{\"format\":\"x\",\"for\\u006dat\":\"y\"}"),&d).isEmpty());
        QVERIFY(!parse(QByteArray("{\"nested\":{\"x\":1,\"x\":2}}"),&d).isEmpty());
        QVERIFY(!parse(QByteArray("{\"text\":\"") + char(0xff) + "\"}",&d).isEmpty());
        QCOMPARE(d.publicationId(),prior);
        auto o = fixture(); o.insert("unknown",true); QVERIFY(!decode(o,&d).isEmpty());
        o = fixture(); o.insert("revision",1.5); QVERIFY(!decode(o,&d).isEmpty());
        o = fixture(); o.insert("schemaVersion","1"); QVERIFY(!decode(o,&d).isEmpty());
        o = fixture(); o.insert("requiredCapabilities",QJsonArray{"calendar.v1","unknown.v1"}); QVERIFY(!decode(o,&d).isEmpty());
        o = fixture(); o.insert("publishedAt","2026-02-30T09:00:00Z"); QVERIFY(!decode(o,&d).isEmpty());
        o = fixture(); o.insert("timeZone","No/SuchZone"); QVERIFY(!decode(o,&d).isEmpty());
        QCOMPARE(d.publicationId(),prior);
    }
    void referencesGlobalIdsAndCoverage()
    {
        Document d; auto o = fixture(); o.insert("assets",QJsonArray{}); QVERIFY(!read(o,&d).isEmpty());
        o = fixture(); auto playlists = o.value("playlists").toArray(); auto second = playlists[1].toObject();
        second.insert("entries",playlists[0].toObject().value("entries")); playlists[1] = second; o.insert("playlists",playlists);
        QVERIFY(!read(o,&d).isEmpty());
        o = fixture(); auto asset = o.value("assets").toArray()[0].toObject(); asset.insert("path","music/../secret.wav");
        o.insert("assets",QJsonArray{asset}); QVERIFY(!read(o,&d).isEmpty());
        o = fixture(); o.insert("baseRules",QJsonArray{base(30,20,10,when({{"type","calendar"},{"calendarId",id(50)}}))});
        o.insert("calendars",QJsonArray{QJsonObject{{"id",id(50)},{"revision",1},{"name","Календарь"},
            {"coverage",QJsonObject{{"from","2026-10-04"},{"until","2026-10-06"}}},{"dates",QJsonArray{}}}});
        QVERIFY(!read(o,&d).isEmpty());
    }
    void overnightCalendarRequiresOnlyTheRelevantPreviousDate()
    {
        auto o = fixture();
        o.insert("baseRules",QJsonArray{base(30,20,10,when({{"type","calendar"},{"calendarId",id(50)}}))});
        QJsonObject calendar{{"id",id(50)},{"revision",1},{"name","Календарь"},
            {"coverage",o.value("validity")},{"dates",QJsonArray{"2026-10-04"}}};
        o.insert("calendars",QJsonArray{calendar});
        Document d; QVERIFY(read(o,&d).isEmpty()); // Midnight full days need no preceding coverage.
        replaceWindow(o,window("22:00:00","02:00:00",1));
        QVERIFY(read(o,&d).contains(QStringLiteral("предыдущую дату")));
        calendar.insert("coverage",QJsonObject{{"from","2026-10-03"},{"until","2026-10-07"}});
        calendar.insert("dates",QJsonArray{"2026-10-03","2026-10-04"}); o.insert("calendars",QJsonArray{calendar});
        QVERIFY(read(o,&d).isEmpty()); QCOMPARE(evaluate(d,at("2026-10-04T01:00:00+03:00")).playlistId,id(11));
    }
};

QTEST_GUILESS_MAIN(ScheduleV1Tests)
#include "tst_schedulev1.moc"
