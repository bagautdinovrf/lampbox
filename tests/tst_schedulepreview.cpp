#include "schedulepreview.h"
#include "advertmodel.h"
#include "channelmodel.h"
#include "schedulecore/schedulev1.h"
#include <QDialog>
#include <QJsonArray>
#include <QLabel>
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

QString id(int value)
{
    return QStringLiteral("00000000-0000-4000-8000-%1").arg(value, 12, 10, QLatin1Char('0'));
}

QJsonObject documentFixture()
{
    const QJsonObject when{{"select", QJsonObject{{"type", "all"}}}, {"excludeDates", QJsonArray{"2026-10-05"}}};
    const auto playlist = [](int number, const QString &name) {
        return QJsonObject{{"id", id(number)}, {"revision", 1}, {"name", name}, {"order", "sequential"},
                           {"entries", QJsonArray{QJsonObject{{"id", id(number + 10)}, {"assetId", id(10)}}}}};
    };
    const auto slot = [](int number, const QString &from, const QString &until, int offset, int playlistNumber) {
        return QJsonObject{{"id", id(number)}, {"window", QJsonObject{{"from", from}, {"until", until}, {"untilDayOffset", offset}}},
                           {"source", QJsonObject{{"type", "playlist"}, {"playlistId", id(playlistNumber)}}}, {"volumePercent", 17}};
    };
    QJsonObject object{
        {"format", "mediabox.schedule"}, {"schemaVersion", 1}, {"scheduleId", id(1)}, {"stationId", id(2)},
        {"publicationId", id(3)}, {"revision", 1}, {"publishedAt", "2026-10-03T09:00:00Z"}, {"timeZone", "Europe/Moscow"},
        {"validity", QJsonObject{{"from", "2026-10-04"}, {"until", "2026-10-07"}}},
        {"musicTransition", "finish_track"}, {"timeResolution", QJsonObject{{"gap", "skip"}, {"overlap", "first"}}},
        {"fallback", QJsonObject{{"source", QJsonObject{{"type", "silence"}}}, {"volumePercent", 0}}},
        {"assets", QJsonArray{QJsonObject{{"id", id(10)}, {"path", QStringLiteral("music/Реклама.wav")}, {"mediaType", "audio"}}}},
        {"playlists", QJsonArray{playlist(11, QStringLiteral("Первый проекта")), playlist(12, QStringLiteral("Второй проекта"))}},
        {"calendars", QJsonArray{}},
        {"dayTemplates", QJsonArray{QJsonObject{{"id", id(30)}, {"name", "День"}, {"slots", QJsonArray{
            slot(31, "00:00:00", "12:00:00", 0, 11), slot(32, "12:00:00", "00:00:00", 1, 12)}}}}},
        {"baseRules", QJsonArray{QJsonObject{{"id", id(40)}, {"name", QStringLiteral("Будни проекта")}, {"enabled", true},
                                           {"priority", 10}, {"when", when}, {"templateId", id(30)}}}},
        {"mixRules", QJsonArray{}},
        {"eventRules", QJsonArray{QJsonObject{{"id", id(50)}, {"name", QStringLiteral("Рекламная вставка")}, {"enabled", true},
                                            {"priority", 0}, {"when", when}, {"times", QJsonArray{"12:15:00"}},
                                            {"action", QJsonObject{{"assetId", id(10)}, {"volumePercent", 80}}},
                                            {"delivery", QJsonObject{{"start", "after_track"}, {"maxLateSeconds", 300},
                                                                     {"expired", "skip"}, {"after", "resume_music"}}}}}}
    };
    QJsonArray capabilities;
    for (const auto &capability : ScheduleV1::requiredCapabilities(object))
        capabilities.append(capability);
    object.insert(QStringLiteral("requiredCapabilities"), capabilities);
    return object;
}

QString tableText(const QTableView *table)
{
    QStringList cells;
    for (int row = 0; row < table->model()->rowCount(); ++row)
        for (int column = 0; column < table->model()->columnCount(); ++column)
            cells.append(table->model()->index(row, column).data().toString());
    return cells.join(QLatin1Char('\n'));
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
    void documentShowsRulesAndEvents();
    void documentGridSelectsAndEditsChannelsById();
    void additionalRulesKeepTheSameGrid();
    void invalidDocumentClearsGrid();
    void overlappingDocumentShowsErrorAndRecovers();
    void externalDocumentErrorClearsPlanAndRecovers();
    void documentDateRefreshAndChannelSelection();
    void documentClockUsesDocumentTimeZone();
    void documentManualDstGapIsReported();
    void boundedLookaheadAndSimultaneousChanges();
    void advertsRespectCalendarAndMinutes();
    void advertFrequencyUsesCompiledMinutesAndNeverIsEmpty();
    void invalidFrequencyHasDiagnostics();
    void preparedMinutesAreUsed();
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

void SchedulePreviewTests::documentShowsRulesAndEvents()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Посторонний канал медиатеки"));
    channel(model, QStringLiteral("Выбранный канал медиатеки"));
    SchedulePreviewWidget widget;
    widget.setModels(&model);
    widget.setSelectedRow(1);
    widget.setPreviewDateTime(at(2026, 10, 4));
    const auto document = documentFixture();
    ScheduleV1::Document compiled;
    const auto error = ScheduleV1::decode(document, &compiled);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    widget.setDocument(document);
    QVERIFY(widget.snapshot().channels.isEmpty());
    QVERIFY(widget.snapshot().activeRows.isEmpty());
    QVERIFY(!widget.snapshot().hasUnresolvedRules);
    QCOMPARE(widget.findChildren<QTableView *>().size(), 1);
    auto *grid = widget.findChild<QWidget *>(QStringLiteral("scheduleDocumentGrid"));
    auto *timeline = widget.findChild<QWidget *>(QStringLiteral("scheduleDocumentTimeline"));
    auto *table = widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable"));
    QVERIFY(grid && timeline && table);
    QVERIFY(!grid->isHidden());
    QVERIFY(!timeline->isHidden());
    QVERIFY(!table->isHidden());
    QCOMPARE(table->editTriggers(), QAbstractItemView::NoEditTriggers);
    const auto text = tableText(table);
    QVERIFY(text.contains(QStringLiteral("Первый проекта")));
    QVERIFY(text.contains(QStringLiteral("Второй проекта")));
    QVERIFY(text.contains(QStringLiteral("12:00")));
    QVERIFY(text.contains(QStringLiteral("17%")));
    QVERIFY(text.contains(QStringLiteral("12:15")));
    QVERIFY(text.contains(QStringLiteral("после трека"), Qt::CaseInsensitive));
    QVERIFY(text.contains(QStringLiteral("Рекламная вставка")));
    QVERIFY(!text.contains(QStringLiteral("Посторонний канал")));
    QVERIFY(!text.contains(QStringLiteral("Базовое правило:")));
    QVERIFY(widget.findChildren<QTextEdit *>().isEmpty());

    QSignalSpy edits(&widget, &SchedulePreviewWidget::editRequested);
    QSignalSpy selections(&widget, &SchedulePreviewWidget::selectedRowChanged);
    table->setCurrentIndex(table->model()->index(0, 0));
    QVERIFY(QMetaObject::invokeMethod(table, "doubleClicked", Qt::DirectConnection,
                                     Q_ARG(QModelIndex, table->currentIndex())));
    QCOMPARE(widget.selectedRow(), 1);
    QCOMPARE(edits.size(), 0);
    QCOMPARE(selections.size(), 0);

    auto *details = widget.findChild<QPushButton *>(QStringLiteral("scheduleDetailsButton"));
    QVERIFY(details);
    QCOMPARE(details->text(), QStringLiteral("Подробности"));
    QString description;
    QTimer::singleShot(0, &widget, [&] {
        if (auto *dialog = widget.findChild<QDialog *>()) {
            if (auto *explanation = dialog->findChild<QTextEdit *>())
                description = explanation->toPlainText();
            dialog->accept();
        }
    });
    details->click();
    QVERIFY(description.contains(QStringLiteral("Базовое правило:")));
    QVERIFY(description.contains(QStringLiteral("Это расчёт плана")));
}

void SchedulePreviewTests::documentGridSelectsAndEditsChannelsById()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Первый проекта"));
    channel(model, QStringLiteral("Канал A"));
    channel(model, QStringLiteral("Канал B"));
    model.setData(model.index(0, 0), id(99), ChannelModel::RuleIdRole);
    model.setData(model.index(1, 0), id(11), ChannelModel::RuleIdRole);
    model.setData(model.index(2, 0), id(12), ChannelModel::RuleIdRole);
    const QSignalSpy dataChanges(&model, &QAbstractItemModel::dataChanged);
    SchedulePreviewWidget widget;
    widget.setModels(&model);
    widget.setPreviewDateTime(at(2026, 10, 4));
    widget.setDocument(documentFixture());
    widget.setSelectedRow(2);
    auto *table = widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable"));
    QVERIFY(table);
    QCOMPARE(table->model()->rowCount(), 3);
    QCOMPARE(table->currentIndex().row(), 1);
    QCOMPARE(table->model()->index(0, 0).data(Qt::UserRole).toString(), id(11));
    QCOMPARE(table->model()->index(1, 0).data(Qt::UserRole).toString(), id(12));
    QSignalSpy selections(&widget, &SchedulePreviewWidget::selectedRowChanged);
    QSignalSpy edits(&widget, &SchedulePreviewWidget::editRequested);

    table->setCurrentIndex(table->model()->index(0, 0));
    QCOMPARE(widget.selectedRow(), 1);
    QCOMPARE(selections.size(), 1);
    QCOMPARE(selections.first().first().toInt(), 1);
    QVERIFY(QMetaObject::invokeMethod(table, "doubleClicked", Qt::DirectConnection,
                                     Q_ARG(QModelIndex, table->currentIndex())));
    QCOMPARE(edits.size(), 1);
    QCOMPARE(edits.first().first().toInt(), 1);
    QCOMPARE(dataChanges.size(), 0);

    // The playlist ID, not either displayed name, identifies the editor row.
    model.setData(model.index(1, 0), QStringLiteral("Переименованный канал"));
    table->setCurrentIndex(table->model()->index(1, 0));
    QCOMPARE(widget.selectedRow(), 2);
    table->setCurrentIndex(table->model()->index(0, 0));
    QCOMPARE(widget.selectedRow(), 1);
    QVERIFY(QMetaObject::invokeMethod(table, "doubleClicked", Qt::DirectConnection,
                                     Q_ARG(QModelIndex, table->currentIndex())));
    QCOMPARE(edits.size(), 2);
    QCOMPARE(edits.last().first().toInt(), 1);
    QCOMPARE(dataChanges.size(), 1);

    model.insertRow(0);
    QCOMPARE(widget.selectedRow(), 2);
    QCOMPARE(table->currentIndex().row(), 0);
    widget.setSelectedRow(3);
    QCOMPARE(table->currentIndex().row(), 1);
    model.removeRow(0);
    QCOMPARE(widget.selectedRow(), 2);
    QCOMPARE(table->currentIndex().row(), 1);

    // Event rows must not accidentally select or edit an unrelated channel.
    selections.clear();
    table->setCurrentIndex(table->model()->index(2, 0));
    QVERIFY(QMetaObject::invokeMethod(table, "doubleClicked", Qt::DirectConnection,
                                     Q_ARG(QModelIndex, table->currentIndex())));
    QCOMPARE(widget.selectedRow(), 2);
    QCOMPARE(selections.size(), 0);
    QCOMPARE(edits.size(), 2);
}

void SchedulePreviewTests::additionalRulesKeepTheSameGrid()
{
    SchedulePreviewWidget widget;
    widget.setPreviewDateTime(at(2026, 10, 4));
    const auto initial = documentFixture();
    widget.setDocument(initial);
    auto *grid = widget.findChild<QWidget *>(QStringLiteral("scheduleDocumentGrid"));
    auto *table = widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable"));
    auto *date = widget.findChild<QDateEdit *>(QStringLiteral("previewDate"));
    auto *time = widget.findChild<QTimeEdit *>(QStringLiteral("previewTime"));
    QVERIFY(grid && table && date && time);
    auto withMix = initial;
    withMix.insert(QStringLiteral("mixRules"), QJsonArray{QJsonObject{
        {"id", id(70)}, {"name", QStringLiteral("Праздничное чередование")}, {"enabled", true}, {"priority", 10},
        {"when", QJsonObject{{"select", QJsonObject{{"type", "all"}}}, {"excludeDates", QJsonArray{}}}},
        {"windows", QJsonArray{QJsonObject{{"from", "00:00:00"}, {"until", "12:00:00"}, {"untilDayOffset", 0}}}},
        {"pattern", QJsonArray{QJsonObject{{"type", "active_base"}},
                              QJsonObject{{"type", "playlist"}, {"playlistId", id(12)}}}},
        {"emptyAdditionalSource", "use_base"}}});
    withMix.insert(QStringLiteral("requiredCapabilities"),
                   QJsonArray::fromStringList(ScheduleV1::requiredCapabilities(withMix)));
    widget.setDocument(withMix);
    QVERIFY2(!widget.snapshot().hasUnresolvedRules, qPrintable(widget.snapshot().issues.join(QLatin1Char('\n'))));
    QVERIFY(tableText(table).contains(QStringLiteral("Чередование")));
    QVERIFY(tableText(table).contains(QStringLiteral("Праздничное чередование")));
    QVERIFY(!grid->isHidden());
    QVERIFY(!date->isHidden());
    QVERIFY(!time->isHidden());
    QCOMPARE(widget.previewDateTime().date(), QDate(2026, 10, 4));
    QCOMPARE(widget.previewDateTime().time(), QTime(10, 0));
    widget.setDocument(initial);
    QVERIFY(!widget.snapshot().hasUnresolvedRules);
    QVERIFY(!tableText(table).contains(QStringLiteral("Чередование")));
    QCOMPARE(widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable")), table);
    QCOMPARE(widget.findChildren<QTableView *>().size(), 1);
    QVERIFY(!grid->isHidden());
    QVERIFY(!date->isHidden());
    QVERIFY(!time->isHidden());
}

void SchedulePreviewTests::invalidDocumentClearsGrid()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Старый канал"));
    SchedulePreviewWidget widget;
    widget.setModels(&model);
    widget.setPreviewDateTime(at(2026, 10, 4));
    widget.setDocument(documentFixture());
    auto *grid = widget.findChild<QWidget *>(QStringLiteral("scheduleDocumentGrid"));
    auto *table = widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable"));
    QVERIFY(grid && table);
    QVERIFY(table->model()->rowCount() > 0);
    widget.setDocument(QJsonObject{});
    QVERIFY(widget.snapshot().hasUnresolvedRules);
    QVERIFY(!widget.snapshot().issues.isEmpty());
    QVERIFY(widget.snapshot().channels.isEmpty());
    QCOMPARE(table->model()->rowCount(), 0);
    QVERIFY(!grid->isHidden());
    QCOMPARE(widget.findChildren<QTableView *>().size(), 1);
    widget.refresh();
    QCOMPARE(table->model()->rowCount(), 0);
    widget.clearDocument();
    QVERIFY(widget.snapshot().channels.isEmpty());
    QVERIFY(widget.snapshot().activeRows.isEmpty());
    QCOMPARE(table->model()->rowCount(), 0);
    QVERIFY(!grid->isHidden());
    QCOMPARE(widget.findChildren<QTableView *>().size(), 1);
    model.setData(model.index(0, 0), QStringLiteral("Изменённый канал"));
    QCOMPARE(table->model()->rowCount(), 0);
    QVERIFY(widget.snapshot().channels.isEmpty());
    widget.setDocument(documentFixture());
    QVERIFY(!widget.snapshot().hasUnresolvedRules);
    QVERIFY(tableText(table).contains(QStringLiteral("Первый проекта")));
    QCOMPARE(widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable")), table);
}

void SchedulePreviewTests::overlappingDocumentShowsErrorAndRecovers()
{
    SchedulePreviewWidget widget;
    widget.setPreviewDateTime(at(2026, 10, 4));
    const auto valid = documentFixture();
    widget.setDocument(valid);
    auto *table = widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable"));
    auto *status = widget.findChild<QLabel *>(QStringLiteral("scheduleStatus"));
    QVERIFY(table && status);
    QVERIFY(table->model()->rowCount() > 0);
    QVERIFY(widget.snapshot().nextChannelTime.isValid());
    QVERIFY(widget.snapshot().nextAdvertTime.isValid());

    auto overlapping = valid;
    auto rules = overlapping.value(QStringLiteral("baseRules")).toArray();
    auto duplicate = rules.first().toObject();
    duplicate.insert(QStringLiteral("id"), id(60));
    duplicate.insert(QStringLiteral("name"), QStringLiteral("Конфликтующий канал"));
    rules.append(duplicate);
    overlapping.insert(QStringLiteral("baseRules"), rules);
    widget.setDocument(overlapping);
    const auto &snapshot = widget.snapshot();
    QVERIFY(snapshot.hasUnresolvedRules);
    QCOMPARE(table->model()->rowCount(), 0);
    QCOMPARE(widget.findChildren<QTableView *>().size(), 1);
    QVERIFY(!status->isHidden());
    QVERIFY(status->property("scheduleIssue").toBool());
    QVERIFY(status->toolTip().contains(QStringLiteral("Будни проекта")));
    QVERIFY(status->toolTip().contains(QStringLiteral("Конфликтующий канал")));
    QVERIFY(status->toolTip().contains(QStringLiteral("одинаковом приоритете")));
    QVERIFY(status->toolTip().contains(QStringLiteral("04.10.2026 00:00:00")));
    QVERIFY(status->toolTip().contains(QStringLiteral("04.10.2026 12:00:00")));
    QVERIFY(status->toolTip().contains(QStringLiteral("Europe/Moscow")));
    QVERIFY(!status->toolTip().contains(id(60)));
    QVERIFY(snapshot.currentSummary.startsWith(QStringLiteral("Расчёт недоступен:")));
    QVERIFY(!snapshot.currentSummary.contains(QStringLiteral("По плану:")));
    QVERIFY(snapshot.nextChannelSummary.contains(QStringLiteral("недоступен")));
    QVERIFY(snapshot.nextAdvertSummary.contains(QStringLiteral("недоступен")));
    QVERIFY(!snapshot.nextChannelTime.isValid());
    QVERIFY(!snapshot.nextAdvertTime.isValid());
    QVERIFY(snapshot.nextChannelNames.isEmpty());
    QVERIFY(snapshot.nextAdvertNames.isEmpty());

    widget.setDocument(valid);
    QVERIFY(!widget.snapshot().hasUnresolvedRules);
    QVERIFY(table->model()->rowCount() > 0);
    QVERIFY(tableText(table).contains(QStringLiteral("Первый проекта")));
    QVERIFY(!status->property("scheduleIssue").toBool());
    QVERIFY(!widget.snapshot().currentSummary.contains(QStringLiteral("недоступен")));
}

void SchedulePreviewTests::externalDocumentErrorClearsPlanAndRecovers()
{
    SchedulePreviewWidget widget;
    widget.setPreviewDateTime(at(2026, 10, 4));
    const auto document = documentFixture();
    widget.setDocument(document);
    auto *table = widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable"));
    auto *status = widget.findChild<QLabel *>(QStringLiteral("scheduleStatus"));
    QVERIFY(table && status);
    QVERIFY(table->model()->rowCount() > 0);
    const QString error = QStringLiteral("Не удалось обновить правила канала «Изменённый».");
    widget.setDocumentError(error);
    QCOMPARE(widget.snapshot().issues, QStringList{error});
    QCOMPARE(table->model()->rowCount(), 0);
    QVERIFY(widget.snapshot().hasUnresolvedRules);
    QVERIFY(widget.snapshot().currentSummary.contains(error));
    QVERIFY(widget.snapshot().nextChannelSummary.contains(QStringLiteral("недоступен")));
    QVERIFY(widget.snapshot().nextAdvertSummary.contains(QStringLiteral("недоступен")));
    QVERIFY(!widget.snapshot().nextChannelTime.isValid());
    QVERIFY(!widget.snapshot().nextAdvertTime.isValid());
    QVERIFY(widget.snapshot().nextChannelNames.isEmpty());
    QVERIFY(widget.snapshot().nextAdvertNames.isEmpty());
    QVERIFY(status->toolTip().contains(error));
    widget.refresh();
    QCOMPARE(table->model()->rowCount(), 0);
    QCOMPARE(widget.snapshot().issues, QStringList{error});

    QString description;
    QTimer::singleShot(0, &widget, [&] {
        if (auto *dialog = widget.findChild<QDialog *>()) {
            if (auto *explanation = dialog->findChild<QTextEdit *>())
                description = explanation->toPlainText();
            dialog->accept();
        }
    });
    widget.showConditions();
    QVERIFY(description.contains(error));
    QVERIFY(!description.contains(QStringLiteral("Правила проекта проверены")));
    QVERIFY(!description.contains(QStringLiteral("Базовое правило:")));

    // The stored JSON has not changed, so recovery must invalidate the decode cache.
    widget.setDocument(document);
    QVERIFY(!widget.snapshot().hasUnresolvedRules);
    QVERIFY(widget.snapshot().issues.isEmpty());
    QVERIFY(table->model()->rowCount() > 0);
    QVERIFY(tableText(table).contains(QStringLiteral("Первый проекта")));
    QVERIFY(!status->property("scheduleIssue").toBool());
    QVERIFY(widget.snapshot().nextChannelTime.isValid());
    QVERIFY(widget.snapshot().nextAdvertTime.isValid());
}

void SchedulePreviewTests::documentDateRefreshAndChannelSelection()
{
    QStandardItemModel model(0, 7);
    channel(model, QStringLiteral("Первый канал"));
    channel(model, QStringLiteral("Сохранённый выбор"));
    SchedulePreviewWidget widget;
    widget.setModels(&model);
    widget.setSelectedRow(1);
    widget.setPreviewDateTime(at(2026, 10, 4));
    widget.setDocument(documentFixture());
    auto *table = widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable"));
    auto *date = widget.findChild<QDateEdit *>(QStringLiteral("previewDate"));
    auto *time = widget.findChild<QTimeEdit *>(QStringLiteral("previewTime"));
    QVERIFY(table && date && time);
    QVERIFY(tableText(table).contains(QStringLiteral("Первый проекта")));
    QCOMPARE(widget.selectedRow(), 1);
    date->setDate(QDate(2026, 10, 5));
    QCOMPARE(widget.snapshot().at.date(), QDate(2026, 10, 5));
    QVERIFY(!tableText(table).contains(QStringLiteral("Первый проекта")));
    QVERIFY(!tableText(table).contains(QStringLiteral("Рекламная вставка")));
    date->setDate(QDate(2026, 10, 4));
    time->setTime(QTime(13, 0));
    QVERIFY(widget.snapshot().currentSummary.contains(QStringLiteral("Второй проекта")));
    QCOMPARE(widget.snapshot().at.timeZone(), QTimeZone("Europe/Moscow"));
    QCOMPARE(widget.snapshot().at.time(), QTime(13, 0));
    model.insertRow(0);
    QCOMPARE(widget.selectedRow(), 2);
    widget.clearDocument();
    QCOMPARE(widget.selectedRow(), 2);
    QVERIFY(widget.snapshot().channels.isEmpty());
    QCOMPARE(table->model()->rowCount(), 0);
    QVERIFY(!table->isHidden());
    QCOMPARE(widget.findChildren<QTableView *>().size(), 1);
}

void SchedulePreviewTests::documentClockUsesDocumentTimeZone()
{
    SchedulePreviewWidget widget;
    auto document = documentFixture();
    document.insert(QStringLiteral("timeZone"), QStringLiteral("Pacific/Kiritimati"));
    widget.setDocument(document);
    const QTimeZone zone("Pacific/Kiritimati");
    const auto now = QDateTime::currentDateTimeUtc().toTimeZone(zone);
    QCOMPARE(widget.previewDateTime().timeZone(), zone);
    QVERIFY(qAbs(widget.previewDateTime().msecsTo(now)) < 2000);
    auto *clock = widget.findChild<QTimer *>(QStringLiteral("scheduleClock"));
    QVERIFY(clock);
    QVERIFY(QMetaObject::invokeMethod(clock, "timeout", Qt::DirectConnection));
    QVERIFY(qAbs(widget.previewDateTime().msecsTo(QDateTime::currentDateTimeUtc())) < 2000);

    widget.setPreviewDateTime(at(2026, 10, 4, 23, 45));
    QCOMPARE(widget.previewDateTime().date(), QDate(2026, 10, 4));
    QCOMPARE(widget.previewDateTime().time(), QTime(23, 45));
    QCOMPARE(widget.previewDateTime().timeZone(), zone);
    document.insert(QStringLiteral("timeZone"), QStringLiteral("Pacific/Honolulu"));
    widget.setDocument(document);
    QCOMPARE(widget.previewDateTime().timeZone(), QTimeZone("Pacific/Honolulu"));
    QCOMPARE(widget.previewDateTime().date(), QDate(2026, 10, 4));
    QCOMPARE(widget.previewDateTime().time(), QTime(23, 45));
    widget.showCurrentTime();
    QVERIFY(qAbs(widget.previewDateTime().msecsTo(QDateTime::currentDateTimeUtc())) < 2000);
}

void SchedulePreviewTests::documentManualDstGapIsReported()
{
    SchedulePreviewWidget widget;
    auto document = documentFixture();
    document.insert(QStringLiteral("timeZone"), QStringLiteral("Europe/Berlin"));
    document.insert(QStringLiteral("validity"), QJsonObject{{"from", "2026-03-28"}, {"until", "2026-03-31"}});
    widget.setPreviewDateTime(at(2026, 3, 29, 1, 30));
    widget.setDocument(document);
    auto *table = widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable"));
    auto *date = widget.findChild<QDateEdit *>(QStringLiteral("previewDate"));
    auto *time = widget.findChild<QTimeEdit *>(QStringLiteral("previewTime"));
    QVERIFY(table && date && time);
    QVERIFY(table->model()->rowCount() > 0);
    QVERIFY(date->toolTip().contains(QStringLiteral("Europe/Berlin")));
    // The editor must retain 02:30 and report the gap instead of silently
    // showing the plan for 01:30 or 03:30 after QDateTime normalization.
    time->setTime(QTime(2, 30));
    QCOMPARE(time->time(), QTime(2, 30));
    QVERIFY(widget.snapshot().hasUnresolvedRules);
    QVERIFY(!widget.snapshot().issues.isEmpty());
    QCOMPARE(table->model()->rowCount(), 0);
    time->setTime(QTime(3, 30));
    QVERIFY(!widget.snapshot().hasUnresolvedRules);
    QVERIFY(table->model()->rowCount() > 0);
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

void SchedulePreviewTests::preparedMinutesAreUsed()
{
    QStandardItemModel model(0, 7);
    advert(model, QStringLiteral("Подготовленная фаза"), QStringLiteral("*"), QStringLiteral("3"),
           QDate(2026, 1, 1), QDate(2026, 12, 31));
    model.setData(model.index(0, 0), QVariantList{2, 22, 42}, AdvertModel::CompiledMinutesRole);
    const auto moment = at(2026, 10, 4, 10, 2);
    auto preview = SchedulePreview::evaluate(nullptr, &model, moment);
    QCOMPARE(preview.exactAdvertsNow, QStringList{QStringLiteral("Подготовленная фаза")});
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
    channel(model, QStringLiteral("Первый проекта"));
    channel(model, QStringLiteral("Второй проекта"), QStringLiteral("12:00"), QStringLiteral("00:00"));
    const QSignalSpy dataChanges(&model, &QAbstractItemModel::dataChanged);
    SchedulePreviewWidget widget;
    widget.setModels(&model);
    widget.setSelectedRow(1);
    widget.setPreviewDateTime(at(2026, 10, 4));
    auto *table = widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable"));
    QVERIFY(table);
    // Editor models keep channel selection, but cannot provide a second plan.
    QCOMPARE(table->model()->rowCount(), 0);
    auto document = documentFixture();
    widget.setDocument(document);
    QCOMPARE(widget.selectedRow(), 1);
    QCOMPARE(dataChanges.size(), 0);
    QCOMPARE(widget.findChildren<QTableView *>().size(), 1);
    QCOMPARE(table->editTriggers(), QAbstractItemView::NoEditTriggers);
    QVERIFY(tableText(table).contains(QStringLiteral("Первый проекта")));
    auto *date = widget.findChild<QDateEdit *>(QStringLiteral("previewDate"));
    auto *time = widget.findChild<QTimeEdit *>(QStringLiteral("previewTime"));
    QVERIFY(date);
    QVERIFY(time);
    date->setDate(QDate(2026, 10, 6));
    time->setTime(QTime(19, 0));
    QVERIFY(widget.snapshot().currentSummary.contains(QStringLiteral("Второй проекта")));
    QCOMPARE(dataChanges.size(), 0);
    model.setData(model.index(1, 0), QStringLiteral("Переименован"));
    QVERIFY(widget.snapshot().currentSummary.contains(QStringLiteral("Второй проекта")));
    QVERIFY(!tableText(table).contains(QStringLiteral("Переименован")));
    auto playlists = document.value(QStringLiteral("playlists")).toArray();
    auto renamed = playlists.at(1).toObject();
    renamed.insert(QStringLiteral("name"), QStringLiteral("Переименован"));
    playlists[1] = renamed;
    document.insert(QStringLiteral("playlists"), playlists);
    widget.setDocument(document);
    QVERIFY(widget.snapshot().currentSummary.contains(QStringLiteral("Переименован")));
    QVERIFY(tableText(table).contains(QStringLiteral("Переименован")));
    QCOMPARE(dataChanges.size(), 1);
    QCOMPARE(widget.selectedRow(), 1);
    model.insertRow(0);
    QCOMPARE(widget.selectedRow(), 2);
    model.removeRow(0);
    QCOMPARE(widget.selectedRow(), 1);
    model.removeRow(0);
    QCOMPARE(widget.selectedRow(), 0);
    QCOMPARE(model.index(widget.selectedRow(), 0).data().toString(), QStringLiteral("Переименован"));
    model.removeRows(0, model.rowCount());
    QVERIFY(widget.snapshot().channels.isEmpty());
    QCOMPARE(widget.selectedRow(), -1);
    // The published plan remains visible until Manager supplies its new document.
    QVERIFY(tableText(table).contains(QStringLiteral("Переименован")));
    QCOMPARE(widget.findChild<QTableView *>(QStringLiteral("scheduleDocumentTable")), table);
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
    widget.setDocument(documentFixture());
    const QTimeZone zone("Europe/Moscow");
    const QDate todayInStation = QDateTime::currentDateTimeUtc().toTimeZone(zone).date();
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
        date->setDate(todayInStation.addDays(-1));
        time->setTime(QTime(23, 59));
    }
    widget.refresh();
    QSignalSpy snapshots(&widget, &SchedulePreviewWidget::snapshotChanged);
    QVERIFY(QMetaObject::invokeMethod(clock, "timeout", Qt::DirectConnection));
    QVERIFY(qAbs(widget.snapshot().at.msecsTo(QDateTime::currentDateTime())) < 2000);
    QCOMPARE(snapshots.size(), 1);
    QCOMPARE(widget.selectedRow(), 1);
    QCOMPARE(dataChanges.size(), 0);

    const auto manual = QDateTime(todayInStation, QTime(3, 15), zone);
    widget.setPreviewDateTime(manual);
    snapshots.clear();
    QVERIFY(QMetaObject::invokeMethod(clock, "timeout", Qt::DirectConnection));
    QCOMPARE(widget.snapshot().at, manual);
    QCOMPARE(snapshots.size(), 0);

    // Clicking today's existing day button returns to the live clock.
    auto *days = widget.findChild<QButtonGroup *>();
    QVERIFY(days);
    auto *today = qobject_cast<QPushButton *>(days->button(todayInStation.dayOfWeek() - 1));
    QVERIFY(today);
    today->click();
    QVERIFY(qAbs(widget.snapshot().at.msecsTo(QDateTime::currentDateTime())) < 2000);
    QCOMPARE(widget.selectedRow(), 1);

    date->setDate(todayInStation.addDays(2));
    time->setTime(QTime(17, 30));
    const auto chosen = widget.previewDateTime();
    QVERIFY(QMetaObject::invokeMethod(clock, "timeout", Qt::DirectConnection));
    QCOMPARE(widget.snapshot().at, chosen);
    QCOMPARE(dataChanges.size(), 0);
}

QTEST_MAIN(SchedulePreviewTests)
#include "tst_schedulepreview.moc"
