#include "schedulecore/schedulecore.h"

#include <QTest>
#include <QTimeZone>
#include <algorithm>

using namespace ScheduleCore;

namespace {
AdvertRule advertRule()
{
    AdvertRule rule;
    rule.name = QStringLiteral("advert.mp3");
    rule.hours = rule.weekdays = QStringLiteral("*");
    rule.from = QDate(2026, 1, 1);
    rule.until = QDate(2026, 12, 31);
    rule.timing = QStringLiteral("3");
    return rule;
}
}

class ScheduleCoreTests final : public QObject
{
    Q_OBJECT
private slots:
    void timingValidation_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("valid");
        for (const QString &text : {QStringLiteral("1"), QStringLiteral("5"), QStringLiteral("*"),
                                   QStringLiteral("0m"), QStringLiteral("59m"), QStringLiteral("00m,30m")})
            QTest::newRow(qPrintable(text)) << text << true;
        for (const QString &text : {QString(), QStringLiteral("0"), QStringLiteral("6"), QStringLiteral("20"),
                                   QStringLiteral("-1"), QStringLiteral("60m"), QStringLiteral("*m"),
                                   QStringLiteral("1m,2"), QStringLiteral("1m,"), QStringLiteral("999999999999")})
            QTest::newRow(qPrintable(QStringLiteral("invalid-") + text)) << text << false;
    }

    void timingValidation()
    {
        QFETCH(QString, text);
        QFETCH(bool, valid);
        const auto parsed = parseAdvertTiming(text);
        QCOMPARE(parsed.valid(), valid);
        QCOMPARE(parsed.error.isEmpty(), valid);
        auto rule = advertRule();
        rule.timing = text;
        QCOMPARE(validateAdvert(rule).isEmpty(), valid);
        const auto preview = evaluate({}, {rule}, QDateTime(rule.from, QTime(0, 0), QTimeZone::UTC));
        QCOMPARE(preview.hasUnresolvedRules, !valid);
    }

    void exactMinutesAreCanonicalAndDisabledIsEmpty()
    {
        auto rule = advertRule();
        rule.timing = QStringLiteral("59m,00m,0m,30m");
        QCOMPARE(compileAdvertMinutes(rule), QList<int>({0, 30, 59}));
        QCOMPARE(formatMinutes(compileAdvertMinutes(rule)), QStringLiteral("0m,30m,59m"));
        rule.timing = QStringLiteral("*");
        QVERIFY(compileAdvertMinutes(rule).isEmpty());
        const auto preview = evaluate({}, {rule}, QDateTime(rule.from, QTime(0, 0), QTimeZone::UTC));
        QVERIFY(preview.exactAdvertsNow.isEmpty());
        QVERIFY(!preview.nextAdvertTime.isValid());
        QVERIFY(!preview.hasUnresolvedRules);
    }

    void frequencyIsStableAcrossNonTemporalEdits()
    {
        auto rule = advertRule();
        const auto original = compileAdvertMinutes(rule);
        // Stable reference guards against accidental rescheduling in future releases.
        QCOMPARE(original, QList<int>({19, 39, 59}));
        for (int volume : {0, 20, 75, 100}) {
            rule.volume = volume;
            QCOMPARE(compileAdvertMinutes(rule), original);
        }
        // Equivalent legacy calendar encodings have the same phase.
        rule.weekdays = QStringLiteral("6,5,4,3,2,1,0,0");
        QCOMPARE(compileAdvertMinutes(rule), original);
        rule.stableId = QStringLiteral("campaign-17");
        const auto identified = compileAdvertMinutes(rule);
        rule.name = QStringLiteral("renamed.mp3");
        QCOMPARE(compileAdvertMinutes(rule), identified);
    }

    void frequencySpacing_data()
    {
        QTest::addColumn<int>("frequency");
        for (int value = 1; value <= 5; ++value)
            QTest::newRow(qPrintable(QString::number(value))) << value;
    }

    void frequencySpacing()
    {
        QFETCH(int, frequency);
        auto rule = advertRule();
        rule.timing = QString::number(frequency);
        // Exercise many stable identities without a random generator or I/O.
        for (int sample = 0; sample < 100; ++sample) {
            rule.stableId = QStringLiteral("campaign-%1").arg(sample);
            const auto minutes = compileAdvertMinutes(rule);
            QCOMPARE(minutes.size(), frequency);
            QVERIFY(minutes.first() >= 0 && minutes.first() < 60 / frequency);
            QVERIFY(minutes.last() < 60);
            for (int index = 1; index < minutes.size(); ++index)
                QCOMPARE(minutes.at(index) - minutes.at(index - 1), 60 / frequency);
            QCOMPARE(compileAdvertMinutes(rule), minutes);
        }
    }

    void emptyAndMixedPlansRemainIndependent()
    {
        const QDateTime at(QDate(2026, 10, 4), QTime(10, 0), QTimeZone::UTC);
        const auto empty = evaluate({}, {}, at);
        QVERIFY(empty.channels.isEmpty());
        QVERIFY(empty.exactAdvertsNow.isEmpty());
        QVERIFY(!empty.nextAdvertTime.isValid());
        QVERIFY(!empty.hasUnresolvedRules);

        QList<AdvertRule> rules;
        for (int frequency = 1; frequency <= 5; ++frequency) {
            auto rule = advertRule();
            rule.stableId = QStringLiteral("rule-%1").arg(frequency);
            rule.name = QStringLiteral("advert-%1.mp3").arg(frequency);
            rule.timing = QString::number(frequency);
            rules.append(rule);
        }
        auto exact = advertRule();
        exact.name = QStringLiteral("exact.mp3");
        exact.timing = QStringLiteral("0m,59m");
        rules.append(exact);
        auto disabled = advertRule();
        disabled.timing = QStringLiteral("*");
        rules.append(disabled);
        const auto original = evaluate({}, rules, at);
        QVERIFY(!original.hasUnresolvedRules);
        QVERIFY(original.exactAdvertsNow.contains(exact.name));
        QVERIFY(original.nextAdvertTime.isValid());
        for (int index = 0; index < 5; ++index)
            QCOMPARE(compileAdvertMinutes(rules[index]).size(), index + 1);
        const auto originalMinutes = compileAdvertMinutes(rules.first());
        std::reverse(rules.begin(), rules.end());
        for (auto &rule : rules)
            rule.volume = 15;
        auto reordered = evaluate({}, rules, at);
        QCOMPARE(reordered.nextAdvertTime, original.nextAdvertTime);
        auto expectedNow = original.exactAdvertsNow;
        expectedNow.sort();
        reordered.exactAdvertsNow.sort();
        QCOMPARE(reordered.exactAdvertsNow, expectedNow);
        QCOMPARE(compileAdvertMinutes(rules.last()), originalMinutes);
    }

    void previewUsesCompiledFrequency()
    {
        const auto rule = advertRule();
        const auto minutes = compileAdvertMinutes(rule);
        const QDateTime at(QDate(2026, 10, 4), QTime(10, minutes.first()), QTimeZone::UTC);
        auto preview = evaluate({}, {rule}, at);
        QCOMPARE(preview.exactAdvertsNow, QStringList{rule.name});
        QCOMPARE(preview.nextAdvertTime, at.addSecs(20 * 60));
        QCOMPARE(preview.frequencyAdvertsNow.size(), 1);
        QVERIFY(!preview.hasUnresolvedRules);
        auto quieter = rule;
        quieter.volume = 12;
        QCOMPARE(evaluate({}, {quieter}, at).nextAdvertTime, preview.nextAdvertTime);
    }

    void publishedLegacyPhaseTakesPrecedence()
    {
        auto rule = advertRule();
        QCOMPARE(compileAdvertMinutes(rule), QList<int>({19, 39, 59}));
        rule.compiledMinutes = {2, 22, 42};
        QVERIFY(validateAdvert(rule).isEmpty());
        QCOMPARE(compileAdvertMinutes(rule), QList<int>({2, 22, 42}));
        const QDateTime at(QDate(2026, 10, 4), QTime(10, 2), QTimeZone::UTC);
        QCOMPARE(evaluate({}, {rule}, at).exactAdvertsNow, QStringList{rule.name});
        QCOMPARE(evaluate({}, {rule}, at).nextAdvertTime, at.addSecs(20 * 60));
        rule.volume = 10;
        QCOMPARE(compileAdvertMinutes(rule), QList<int>({2, 22, 42}));
        rule.compiledMinutes = {2, 22, 60};
        QVERIFY(!validateAdvert(rule).isEmpty());
        QVERIFY(compileAdvertMinutes(rule).isEmpty());
        QVERIFY(evaluate({}, {rule}, at).hasUnresolvedRules);
        rule.compiledMinutes = {2, 22, 42};
        rule.timing = QStringLiteral("2");
        QVERIFY(!validateAdvert(rule).isEmpty());
    }

    void completeRuleValidation()
    {
        auto rule = advertRule();
        rule.until = rule.from.addDays(-1);
        QVERIFY(!validateAdvert(rule).isEmpty());
        rule = advertRule();
        rule.hours = QStringLiteral("24");
        QVERIFY(!validateAdvert(rule).isEmpty());
        rule = advertRule();
        rule.volume = 101;
        QVERIFY(!validateAdvert(rule).isEmpty());
        ChannelRule channel{QStringLiteral("Music"), QStringLiteral("*"), QStringLiteral("*"),
                            QStringLiteral("*"), QTime(8, 0), QTime(18, 0), 50};
        QVERIFY(validateChannel(channel).isEmpty());
        const QDateTime start(QDate(2026, 10, 4), QTime(8, 0), QTimeZone::UTC);
        QCOMPARE(evaluate({channel}, {}, start).activeRows, QList<int>{0});
        QVERIFY(evaluate({channel}, {}, start.addSecs(10 * 3600)).activeRows.isEmpty());
        channel.start = channel.end; // Legacy data remains readable, not interpreted as 24h.
        QVERIFY(validateChannel(channel).isEmpty());
        QVERIFY(evaluate({channel}, {}, start).hasUnresolvedRules);
        channel.months = QStringLiteral("13");
        QVERIFY(!validateChannel(channel).isEmpty());
    }
};

QTEST_GUILESS_MAIN(ScheduleCoreTests)
#include "tst_schedulecore.moc"
