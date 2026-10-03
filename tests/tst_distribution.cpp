#include <QSet>
#include <QStringList>
#include <QTest>

#include "trackdistributor.h"

class DistributionTest final : public QObject
{
    Q_OBJECT

private slots:
    void hourlyFrequency_data()
    {
        QTest::addColumn<int>("frequency");
        for (int frequency = 1; frequency <= 5; ++frequency)
            QTest::newRow(qPrintable(QString::number(frequency))) << frequency;
    }

    void hourlyFrequency()
    {
        QFETCH(int, frequency);
        // Check the schedule contract across many random starting minutes.
        for (int sample = 0; sample < 100; ++sample) {
            TracksFullInfo tracks(1);
            tracks.front().mTrackFrequency = QString::number(frequency);
            tracks.front().mFileName = QStringLiteral("advert.mp3");
            TrackDistributor distributor(tracks);
            QVERIFY(&distributor.distribute() == &tracks);

            const QStringList minutes = tracks.front().minuts().split(',');
            QCOMPARE(minutes.size(), qsizetype(frequency));
            const int step = 60 / frequency;
            int previous = -1;
            QSet<int> seen;
            for (const QString &entry : minutes) {
                QVERIFY(entry.endsWith('m'));
                bool ok = false;
                const int minute = entry.chopped(1).toInt(&ok);
                QVERIFY(ok);
                QVERIFY(minute >= 0 && minute < 60);
                QVERIFY(!seen.contains(minute));
                seen.insert(minute);
                if (previous == -1)
                    QVERIFY(minute < step - 1);
                else
                    QCOMPARE(minute - previous, step);
                previous = minute;
            }
            QCOMPARE(tracks.front().mFileName, QStringLiteral("advert.mp3"));
        }
    }

    void fixedMinutes_data()
    {
        QTest::addColumn<QString>("minutes");
        QTest::newRow("first-minute") << QStringLiteral("0m");
        QTest::newRow("last-minute") << QStringLiteral("59m");
        QTest::newRow("multiple-minutes") << QStringLiteral("5m,30m,59m");
    }

    void fixedMinutes()
    {
        QFETCH(QString, minutes);
        TracksFullInfo tracks(1);
        tracks.front().mTrackFrequency = minutes;
        TrackDistributor distributor(tracks);
        distributor.distribute();
        QCOMPARE(tracks.front().minuts(), minutes);
    }

    void mixedSchedules()
    {
        TracksFullInfo tracks(6);
        for (int frequency = 1; frequency <= 5; ++frequency)
            tracks[frequency - 1].mTrackFrequency = QString::number(frequency);
        tracks.back().mTrackFrequency = QStringLiteral("0m,59m");

        TrackDistributor distributor(tracks);
        distributor.distribute();
        for (int frequency = 1; frequency <= 5; ++frequency)
            QCOMPARE(tracks[frequency - 1].minuts().split(',').size(), qsizetype(frequency));
        QCOMPARE(tracks.back().minuts(), QStringLiteral("0m,59m"));
    }

    void invalidFrequency_data()
    {
        QTest::addColumn<QString>("frequency");
        QTest::newRow("empty") << QString();
        QTest::newRow("disabled") << QStringLiteral("0");
        QTest::newRow("negative") << QStringLiteral("-1");
        QTest::newRow("above-editor-limit") << QStringLiteral("6");
        QTest::newRow("zero-random-bound") << QStringLiteral("59");
        QTest::newRow("one-minute-step") << QStringLiteral("60");
        QTest::newRow("zero-minute-step") << QStringLiteral("61");
        QTest::newRow("overflow") << QStringLiteral("9999999999");
        QTest::newRow("non-number") << QStringLiteral("invalid");
    }

    void invalidFrequency()
    {
        QFETCH(QString, frequency);
        TracksFullInfo tracks(1);
        tracks.front().mTrackFrequency = frequency;
        TrackDistributor distributor(tracks);
        distributor.distribute();
        QVERIFY(tracks.front().minuts().isEmpty());
    }

    void emptyTracks()
    {
        TracksFullInfo tracks;
        TrackDistributor distributor(tracks);
        QVERIFY(&distributor.distribute() == &tracks);
        QVERIFY(tracks.empty());
    }
};

QTEST_GUILESS_MAIN(DistributionTest)
#include "tst_distribution.moc"
