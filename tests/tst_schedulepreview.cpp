#include "schedulepreview.h"
#include "advertmodel.h"
#include "channelmodel.h"
#include <QTextEdit>

#include <QDateEdit>
#include <QButtonGroup>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTableView>
#include <QTest>
#include <QTimeEdit>
#include <QTimer>
#include <QTimeZone>

namespace {
void channel(QStandardItemModel &model, const QString &name, const QString &start = QStringLiteral("08:00"),
             const QString &end = QStringLiteral("18:00"), const QString &weekdays = QStringLiteral("*"),
             const QString &days = QStringLiteral("*"), const QString &months = QStringLiteral("*"))
{
    const int row = model.rowCount();
    model.insertRow(row);
    const QList<QVariant> fields{name, start, end, weekdays, days, months, 100};
    for (int column = 0; column < fields.size(); ++column)
        model.setData(model.index(row, column), fields.at(column));
}

void advert(QStandardItemModel &model, const QString &name, const QString &hours, const QString &minutes,
            const QDate &from, const QDate &until, const QString &weekdays = QStringLiteral("*"))
{
    const int row = model.rowCount();
    model.insertRow(row);
    const QList<QVariant> fields{name, hours, minutes, weekdays, from, until, 100};
    for (int column = 0; column < fields.size(); ++column)
        model.setData(model.index(row, column), fields.at(column));
}

QDateTime at(int year, int month, int day, int hour = 10, int minute = 0)
{
    return QDateTime(QDate(year, month, day), QTime(hour, minute), QTimeZone::UTC);
}
}

class SchedulePreviewTests : public QObject
{
    Q_OBJECT
private slots:
    void cronWeekdaysAndMonths();
    void leapAndInvalidCalendar();
    void intervalBoundsAndAmbiguity();
    void unsupportedIntervals();
    void explicitFullDayAndNightUseModelRole();
    void advancedDocumentHidesLegacyPlan();
    void boundedLookaheadAndSimultaneousChanges();
    void advertsRespectCalendarAndMinutes();
    void advertFrequencyUsesCompiledMinutesAndNeverIsEmpty();
    void invalidFrequencyHasDiagnostics();
    void publishedLegacyMinutesAreUsed();
    void localTimeTransitionsDoNotInventExactEvents();
    void widgetIsReadOnlyAndRefreshes();
    void clockFollowsCurrentTimeAndPreservesManualPreview();
};

void SchedulePreviewTests::cronWeekdaysAndMonths()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Воскресенье"), QStringLiteral("08:00"), QStringLiteral("18:00"),
            QStringLiteral("0"), QStringLiteral("4"), QStringLiteral("10"));
    auto sunday = SchedulePreview::evaluate(&model, nullptr, at(2026, 10, 4));
    QCOMPARE(sunday.activeRows, QList<int>{0});
    QVERIFY(sunday.channels.first().calendarMatches);
    const auto monday = SchedulePreview::evaluate(&model, nullptr, at(2026, 10, 5));
    QVERIFY(monday.activeRows.isEmpty());
    QVERIFY(monday.channels.first().reason.contains(QStringLiteral("День недели")));
    const auto november = SchedulePreview::evaluate(&model, nullptr, at(2026, 11, 1));
    QVERIFY(november.activeRows.isEmpty());
    QVERIFY(november.channels.first().reason.contains(QStringLiteral("Месяц")));
}

void SchedulePreviewTests::leapAndInvalidCalendar()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Високосный"), QStringLiteral("08:00"), QStringLiteral("18:00"),
            QStringLiteral("*"), QStringLiteral("29"), QStringLiteral("2"));
    const auto leap = SchedulePreview::evaluate(&model, nullptr, at(2028, 2, 28, 19));
    QCOMPARE(leap.nextChannelTime, at(2028, 2, 29, 8));
    QCOMPARE(SchedulePreview::evaluate(&model, nullptr, at(2028, 2, 29)).activeRows, QList<int>{0});
    QVERIFY(!SchedulePreview::evaluate(&model, nullptr, at(2029, 2, 28, 19)).nextChannelTime.isValid());
    model.setData(model.index(0, 4), QStringLiteral("31"));
    QVERIFY(!SchedulePreview::evaluate(&model, nullptr, at(2028, 2, 1)).nextChannelTime.isValid());
    model.setData(model.index(0, 5), QStringLiteral("0"));
    const auto invalid = SchedulePreview::evaluate(&model, nullptr, at(2028, 2, 1));
    QVERIFY(invalid.hasUnresolvedRules);
    QVERIFY(!invalid.channels.first().valid);
    QVERIFY(SchedulePreview::evaluate(&model, nullptr, QDateTime()).channels.isEmpty());
}

void SchedulePreviewTests::intervalBoundsAndAmbiguity()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Утро"), QStringLiteral("08:00"), QStringLiteral("12:00"));
    channel(model, QStringLiteral("День"), QStringLiteral("12:00"), QStringLiteral("18:00"));
    QCOMPARE(SchedulePreview::evaluate(&model, nullptr, at(2026, 10, 4, 8)).activeRows, QList<int>{0});
    const auto boundary = SchedulePreview::evaluate(&model, nullptr, at(2026, 10, 4, 12));
    QCOMPARE(boundary.activeRows, QList<int>{1});
    QVERIFY(boundary.issues.isEmpty());
    QVERIFY(SchedulePreview::evaluate(&model, nullptr, at(2026, 10, 4, 18)).activeRows.isEmpty());
    model.setData(model.index(1, 1), QStringLiteral("11:00"));
    const auto overlap = SchedulePreview::evaluate(&model, nullptr, at(2026, 10, 4, 11, 30));
    QCOMPARE(overlap.activeRows, QList<int>({0, 1}));
    QVERIFY(overlap.currentSummary.startsWith(QStringLiteral("Пересечение")));
    QVERIFY(overlap.issues.first().contains(QStringLiteral("Приоритет не задан")));
    model.setData(model.index(1, 1), QStringLiteral("13:00"));
    const auto gap = SchedulePreview::evaluate(&model, nullptr, at(2026, 10, 4, 12, 30));
    QVERIFY(gap.activeRows.isEmpty());
    QVERIFY(gap.issues.first().contains(QStringLiteral("нет назначенного канала")));
}

void SchedulePreviewTests::unsupportedIntervals()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Ночь"), QStringLiteral("22:00"), QStringLiteral("06:00"));
    channel(model, QStringLiteral("Равные"), QStringLiteral("00:00"), QStringLiteral("00:00"));
    channel(model, QStringLiteral("Ошибка"), QStringLiteral("24:00"), QStringLiteral("06:00"));
    const auto result = SchedulePreview::evaluate(&model, nullptr, at(2026, 12, 31, 23));
    QVERIFY(result.activeRows.isEmpty());
    QVERIFY(result.hasUnresolvedRules);
    QVERIFY(!result.nextChannelTime.isValid());
    QCOMPARE(result.issues.size(), 3);
    QVERIFY(result.channels.at(0).reason.contains(QStringLiteral("полночь")));
    QVERIFY(result.channels.at(1).reason.contains(QStringLiteral("Полные сутки")));
    QVERIFY(result.channels.at(2).reason.contains(QStringLiteral("Некорректное время")));
}

void SchedulePreviewTests::explicitFullDayAndNightUseModelRole()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Сутки"), QStringLiteral("00:00"), QStringLiteral("00:00"), QStringLiteral("1"));
    model.setData(model.index(0, 0), 1, ChannelModel::UntilDayOffsetRole);
    const auto day = SchedulePreview::evaluate(&model, nullptr, at(2026, 10, 5, 23, 59));
    QCOMPARE(day.activeRows, QList<int>{0});
    QCOMPARE(day.channels.first().dayIntervals, (QList<QPair<int, int>>{{0, 1440}}));
    QVERIFY(SchedulePreview::evaluate(&model, nullptr, at(2026, 10, 6, 0)).activeRows.isEmpty());
    model.setData(model.index(0, 1), QStringLiteral("22:00"));
    model.setData(model.index(0, 2), QStringLiteral("06:00"));
    const auto tail = SchedulePreview::evaluate(&model, nullptr, at(2026, 10, 6, 2));
    QCOMPARE(tail.activeRows, QList<int>{0});
    QCOMPARE(tail.channels.first().dayIntervals, (QList<QPair<int, int>>{{0, 360}}));
}

void SchedulePreviewTests::advancedDocumentHidesLegacyPlan()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Старый"));
    SchedulePreviewWidget widget;
    widget.setModels(&model);
    widget.setDocument(QJsonObject{});
    QVERIFY(widget.snapshot().channels.isEmpty());
    QVERIFY(widget.snapshot().hasUnresolvedRules);
    QVERIFY(widget.findChild<QTableView *>(QStringLiteral("scheduleTimeline"))->isHidden());
    auto *advanced = widget.findChild<QTextEdit *>(QStringLiteral("scheduleDocumentPlan"));
    QVERIFY(advanced && !advanced->isHidden());
    QVERIFY(advanced->toPlainText().contains(QStringLiteral("исправления")));
    widget.clearDocument();
    QCOMPARE(widget.snapshot().channels.size(), 1);
    QVERIFY(advanced->isHidden());
}

void SchedulePreviewTests::boundedLookaheadAndSimultaneousChanges()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Утро"), QStringLiteral("08:00"), QStringLiteral("12:00"));
    channel(model, QStringLiteral("День"), QStringLiteral("12:00"), QStringLiteral("18:00"));
    const auto simultaneous = SchedulePreview::evaluate(&model, nullptr, at(2026, 12, 31, 11));
    QCOMPARE(simultaneous.nextChannelTime, at(2026, 12, 31, 12));
    QCOMPARE(simultaneous.nextChannelNames.size(), 2);
    QVERIFY(!SchedulePreview::evaluate(&model, nullptr, at(2026, 12, 31, 20), 1).nextChannelTime.isValid());
    QCOMPARE(SchedulePreview::evaluate(&model, nullptr, at(2026, 12, 31, 20), 2).nextChannelTime, at(2027, 1, 1, 8));
    QCOMPARE(SchedulePreview::evaluate(&model, nullptr, at(2026, 12, 31), 999999).horizonDays, 366);
    QCOMPARE(SchedulePreview::evaluate(&model, nullptr, at(2026, 12, 31), -1).horizonDays, 1);
}

void SchedulePreviewTests::advertsRespectCalendarAndMinutes()
{
    QStandardItemModel model(0, 7);
    advert(model, QStringLiteral("Реклама A"), QStringLiteral("10,11"), QStringLiteral("00m,30m"),
           QDate(2026, 10, 4), QDate(2026, 10, 4), QStringLiteral("0"));
    advert(model, QStringLiteral("Реклама B"), QStringLiteral("11"), QStringLiteral("00m"),
           QDate(2026, 10, 4), QDate(2026, 10, 4), QStringLiteral("0"));
    auto result = SchedulePreview::evaluate(nullptr, &model, at(2026, 10, 4, 10, 30));
    QCOMPARE(result.exactAdvertsNow, QStringList{QStringLiteral("Реклама A")});
    QCOMPARE(result.nextAdvertTime, at(2026, 10, 4, 11));
    QCOMPARE(result.nextAdvertNames.size(), 2);
    QVERIFY(!SchedulePreview::evaluate(nullptr, &model, at(2026, 10, 4, 11, 31)).nextAdvertTime.isValid());
    QVERIFY(SchedulePreview::evaluate(nullptr, &model, at(2026, 10, 5, 10)).exactAdvertsNow.isEmpty());
    model.setData(model.index(0, 2), QStringLiteral("60m"));
    result = SchedulePreview::evaluate(nullptr, &model, at(2026, 10, 4, 10));
    QVERIFY(result.hasUnresolvedRules);
    QVERIFY(result.exactAdvertsNow.isEmpty());
}

void SchedulePreviewTests::advertFrequencyUsesCompiledMinutesAndNeverIsEmpty()
{
    QStandardItemModel model(0, 7);
    advert(model, QStringLiteral("Частота"), QStringLiteral("*"), QStringLiteral("3"),
           QDate(2026, 1, 1), QDate(2026, 12, 31));
    advert(model, QStringLiteral("Никогда"), QStringLiteral("*"), QStringLiteral("*"),
           QDate(2026, 1, 1), QDate(2026, 12, 31));
    ScheduleCore::AdvertRule rule;
    rule.name = QStringLiteral("Частота");
    rule.hours = rule.weekdays = QStringLiteral("*");
    rule.timing = QStringLiteral("3");
    rule.from = QDate(2026, 1, 1);
    rule.until = QDate(2026, 12, 31);
    const auto minutes = ScheduleCore::compileAdvertMinutes(rule);
    const auto moment = at(2026, 10, 4, 10, minutes.first());
    const auto result = SchedulePreview::evaluate(nullptr, &model, moment);
    QCOMPARE(result.frequencyAdvertsNow.size(), 1);
    QCOMPARE(result.exactAdvertsNow, QStringList{QStringLiteral("Частота")});
    QCOMPARE(result.nextAdvertTime, moment.addSecs(20 * 60));
    QVERIFY(!result.hasUnresolvedRules);
    model.setData(model.index(0, 6), 15);
    QCOMPARE(SchedulePreview::evaluate(nullptr, &model, moment).nextAdvertTime, result.nextAdvertTime);
    model.setData(model.index(0, 2), QStringLiteral("*"));
    const auto disabled = SchedulePreview::evaluate(nullptr, &model, moment);
    QVERIFY(disabled.exactAdvertsNow.isEmpty());
    QVERIFY(!disabled.nextAdvertTime.isValid());
}

void SchedulePreviewTests::invalidFrequencyHasDiagnostics()
{
    for (const auto &timing : {QStringLiteral("6"), QStringLiteral("20"), QStringLiteral("60m")}) {
        QStandardItemModel model(0, 7);
        advert(model, QStringLiteral("Некорректная"), QStringLiteral("*"), timing,
               QDate(2026, 1, 1), QDate(2026, 12, 31));
        const auto result = SchedulePreview::evaluate(nullptr, &model, at(2026, 10, 4));
        QVERIFY(result.hasUnresolvedRules);
        QVERIFY(!result.issues.isEmpty());
        QVERIFY(result.frequencyAdvertsNow.isEmpty());
        QVERIFY(result.exactAdvertsNow.isEmpty());
        QVERIFY(!result.nextAdvertTime.isValid());
    }
}

void SchedulePreviewTests::publishedLegacyMinutesAreUsed()
{
    QStandardItemModel model(0, 7);
    advert(model, QStringLiteral("Старая фаза"), QStringLiteral("*"), QStringLiteral("3"),
           QDate(2026, 1, 1), QDate(2026, 12, 31));
    model.setData(model.index(0, 0), QVariantList{2, 22, 42}, AdvertModel::CompiledMinutesRole);
    const auto moment = at(2026, 10, 4, 10, 2);
    auto preview = SchedulePreview::evaluate(nullptr, &model, moment);
    QCOMPARE(preview.exactAdvertsNow, QStringList{QStringLiteral("Старая фаза")});
    QCOMPARE(preview.nextAdvertTime, moment.addSecs(20 * 60));
    model.setData(model.index(0, 6), 10);
    QCOMPARE(SchedulePreview::evaluate(nullptr, &model, moment).nextAdvertTime, preview.nextAdvertTime);
    model.setData(model.index(0, 0), QVariantList{2, 22, 60}, AdvertModel::CompiledMinutesRole);
    preview = SchedulePreview::evaluate(nullptr, &model, moment);
    QVERIFY(preview.hasUnresolvedRules);
    QVERIFY(preview.exactAdvertsNow.isEmpty());
    QVERIFY(!preview.nextAdvertTime.isValid());
}

void SchedulePreviewTests::localTimeTransitionsDoNotInventExactEvents()
{
    const QTimeZone zone("Europe/Berlin");
    QVERIFY(zone.isValid());
    QStandardItemModel model(0, 7);
    advert(model, QStringLiteral("Точная минута"), QStringLiteral("2"), QStringLiteral("30m"),
           QDate(2026, 1, 1), QDate(2026, 12, 31));
    // 02:30 does not exist on the spring transition date. Qt's default
    // transition policy normalizes it to 03:30; that is not the saved minute.
    auto result = SchedulePreview::evaluate(nullptr, &model,
            QDateTime(QDate(2026, 3, 29), QTime(1, 0), zone));
    QCOMPARE(result.nextAdvertTime, QDateTime(QDate(2026, 3, 30), QTime(2, 30), zone));
    // The station contract uses the first occurrence of repeated wall time.
    result = SchedulePreview::evaluate(nullptr, &model,
            QDateTime(QDate(2026, 10, 25), QTime(1, 0), zone));
    const auto firstOccurrence = QDateTime::fromMSecsSinceEpoch(
            QDateTime(QDate(2026, 10, 25), QTime(0, 30), QTimeZone::UTC).toMSecsSinceEpoch(), zone);
    QCOMPARE(result.nextAdvertTime, firstOccurrence);
    QCOMPARE(SchedulePreview::evaluate(nullptr, &model, firstOccurrence.addSecs(30)).exactAdvertsNow,
             QStringList{QStringLiteral("Точная минута")});
    const auto repeated = SchedulePreview::evaluate(nullptr, &model, firstOccurrence.addSecs(3600));
    QVERIFY(repeated.exactAdvertsNow.isEmpty());
    QCOMPARE(repeated.nextAdvertTime, QDateTime(QDate(2026, 10, 26), QTime(2, 30), zone));
    QStandardItemModel channels(0, 7);
    channel(channels, QStringLiteral("Несуществующее начало"), QStringLiteral("02:30"), QStringLiteral("04:00"));
    result = SchedulePreview::evaluate(&channels, nullptr,
            QDateTime(QDate(2026, 3, 29), QTime(3, 15), zone));
    QVERIFY(result.activeRows.isEmpty());
    QVERIFY(result.hasUnresolvedRules);
    QCOMPARE(result.nextChannelTime, QDateTime(QDate(2026, 3, 30), QTime(2, 30), zone));
}

void SchedulePreviewTests::widgetIsReadOnlyAndRefreshes()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Первый"));
    channel(model, QStringLiteral("Второй"), QStringLiteral("18:00"), QStringLiteral("23:00"));
    const QSignalSpy dataChanges(&model, &QAbstractItemModel::dataChanged);
    SchedulePreviewWidget widget;
    widget.setModels(&model);
    widget.setSelectedRow(1);
    widget.setPreviewDateTime(at(2026, 10, 4));
    QCOMPARE(widget.selectedRow(), 1);
    QCOMPARE(dataChanges.size(), 0);
    const auto tables = widget.findChildren<QTableView *>();
    QCOMPARE(tables.size(), 2);
    for (const auto *table : tables) {
        QCOMPARE(table->currentIndex().row(), 1);
        QCOMPARE(table->editTriggers(), QAbstractItemView::NoEditTriggers);
    }
    auto *date = widget.findChild<QDateEdit *>(QStringLiteral("previewDate"));
    auto *time = widget.findChild<QTimeEdit *>(QStringLiteral("previewTime"));
    QVERIFY(date);
    QVERIFY(time);
    date->setDate(QDate(2027, 1, 1));
    time->setTime(QTime(19, 0));
    QCOMPARE(widget.snapshot().activeRows, QList<int>{1});
    QCOMPARE(dataChanges.size(), 0);
    model.setData(model.index(1, 0), QStringLiteral("Переименован"));
    QCOMPARE(widget.snapshot().channels.at(1).name, QStringLiteral("Переименован"));
    QCOMPARE(widget.selectedRow(), 1);
    model.insertRow(0);
    QCOMPARE(widget.selectedRow(), 2);
    for (const auto *table : tables)
        QCOMPARE(table->currentIndex().row(), 2);
    model.removeRow(0);
    QCOMPARE(widget.selectedRow(), 1);
    model.removeRow(0);
    QCOMPARE(widget.selectedRow(), 0);
    QCOMPARE(widget.snapshot().channels.at(widget.selectedRow()).name, QStringLiteral("Переименован"));
    for (const auto *table : tables)
        QCOMPARE(table->currentIndex().row(), 0);
    model.removeRows(0, model.rowCount());
    QVERIFY(widget.snapshot().channels.isEmpty());
    QCOMPARE(widget.selectedRow(), -1);
}

void SchedulePreviewTests::clockFollowsCurrentTimeAndPreservesManualPreview()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Первый"));
    channel(model, QStringLiteral("Второй"));
    const QSignalSpy dataChanges(&model, &QAbstractItemModel::dataChanged);
    SchedulePreviewWidget widget;
    widget.setModels(&model);
    widget.setSelectedRow(1);
    auto *clock = widget.findChild<QTimer *>(QStringLiteral("scheduleClock"));
    auto *date = widget.findChild<QDateEdit *>(QStringLiteral("previewDate"));
    auto *time = widget.findChild<QTimeEdit *>(QStringLiteral("previewTime"));
    QVERIFY(clock && date && time);
    QVERIFY(clock->isActive());
    QCOMPARE(clock->interval(), 60000);

    // Simulate the clock crossing midnight without changing the system clock
    // or waiting a minute. Programmatic refresh must not freeze live mode.
    {
        const QSignalBlocker dateBlocker(date), timeBlocker(time);
        date->setDate(QDate::currentDate().addDays(-1));
        time->setTime(QTime(23, 59));
    }
    widget.refresh();
    QSignalSpy snapshots(&widget, &SchedulePreviewWidget::snapshotChanged);
    QVERIFY(QMetaObject::invokeMethod(clock, "timeout", Qt::DirectConnection));
    QVERIFY(qAbs(widget.snapshot().at.msecsTo(QDateTime::currentDateTime())) < 2000);
    QCOMPARE(snapshots.size(), 1);
    QCOMPARE(widget.selectedRow(), 1);
    QCOMPARE(dataChanges.size(), 0);

    const auto manual = QDateTime(QDate::currentDate(), QTime(3, 15));
    widget.setPreviewDateTime(manual);
    snapshots.clear();
    QVERIFY(QMetaObject::invokeMethod(clock, "timeout", Qt::DirectConnection));
    QCOMPARE(widget.snapshot().at, manual);
    QCOMPARE(snapshots.size(), 0);

    // Clicking today's existing day button returns to the live clock.
    auto *days = widget.findChild<QButtonGroup *>();
    QVERIFY(days);
    auto *today = qobject_cast<QPushButton *>(days->button(QDate::currentDate().dayOfWeek() - 1));
    QVERIFY(today);
    today->click();
    QVERIFY(qAbs(widget.snapshot().at.msecsTo(QDateTime::currentDateTime())) < 2000);
    QCOMPARE(widget.selectedRow(), 1);

    date->setDate(QDate::currentDate().addDays(2));
    time->setTime(QTime(17, 30));
    const auto chosen = widget.previewDateTime();
    QVERIFY(QMetaObject::invokeMethod(clock, "timeout", Qt::DirectConnection));
    QCOMPARE(widget.snapshot().at, chosen);
    QCOMPARE(dataChanges.size(), 0);
}

QTEST_MAIN(SchedulePreviewTests)
#include "tst_schedulepreview.moc"
