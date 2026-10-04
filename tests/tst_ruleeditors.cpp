#include "projectfixture.h"
#include "ruleeditors.h"
#include "restyletheme.h"
#include "channelmodel.h"
#include "scheduledocumentdialog.h"
#include "schedulecore/schedulev1.h"
#include <QCheckBox>
#include <QTimeEdit>
#include <QPlainTextEdit>
#include <QJsonDocument>

#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTimeZone>
#include <QToolButton>

using namespace RuleEditors;

namespace {
class RecordingModel : public QStandardItemModel {
public:
    int writes = 0;
    int refuseColumn = -1;
    bool refused = false;

    void append(const QList<QVariant> &values)
    {
        QList<QStandardItem *> items;
        for (const QVariant &value : values) {
            auto *item = new QStandardItem;
            item->setData(value, Qt::EditRole);
            items << item;
        }
        appendRow(items);
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role) override
    {
        ++writes;
        if (index.column() == refuseColumn && !refused) {
            refused = true;
            return false;
        }
        return QStandardItemModel::setData(index, value, role);
    }
};

QList<QVariant> channelRow(const QString &name = QStringLiteral("Утро"))
{
    return {name, QTime(8, 0), QTime(12, 0), QStringLiteral("*"), QStringLiteral("*"), QStringLiteral("*"), 65};
}

QList<QVariant> advertRow()
{
    return {QStringLiteral("Ролик.mp3"), QStringLiteral("*"), QStringLiteral("00m,30m"), QStringLiteral("*"),
            QDate(2026, 10, 1), QDate(2026, 10, 31), 75};
}

QList<QVariant> snapshot(QAbstractItemModel &model, int row = 0)
{
    QList<QVariant> result;
    for (int col = 0; col < model.columnCount(); ++col) result << model.index(row, col).data(Qt::EditRole);
    return result;
}
}

class RuleEditorsTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        Restyle::install(*qApp);
    }

    void realRuleThroughProxyCommitsOnceAndRetainsStateOnWriteFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const bool originalTestMode = QStandardPaths::isTestModeEnabled();
        const QVariant originalStation = qApp->property("restylePreviewStation");
        const QVariant originalSettings = qApp->property("restylePreviewSettings");
        const auto restore = qScopeGuard([&] {
            qApp->setProperty("restylePreviewStation", originalStation);
            qApp->setProperty("restylePreviewSettings", originalSettings);
            QStandardPaths::setTestModeEnabled(originalTestMode);
        });
        QStandardPaths::setTestModeEnabled(true);
        qApp->setProperty("restylePreviewStation", directory.path());
        qApp->setProperty("restylePreviewSettings", directory.filePath("manager.conf"));
        const auto write = [](const QString &path, const QByteArray &bytes) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
        };
        QVERIFY(write(directory.filePath("mediabox.conf"),
                      "[mediastation]\nmediabox_id=-1\nmediabox_name=Test\nmedia=media\ncrondir=cron\n"));
        QVERIFY(QDir().mkpath(directory.filePath("media/music/А")));
        QVERIFY(QDir().mkpath(directory.filePath("media/music/Б")));
        ProjectRepository::Project project;
        project.music = {ProjectFixture::channel("А", QTime(8, 0), QTime(12, 0), 65),
                         ProjectFixture::channel("Б", QTime(12, 0), QTime(18, 0), 70)};
        const QString path = directory.filePath("project.json");
        QVERIFY(write(path, ProjectRepository::encode(project)));
        ChannelManager manager(MUSIC);
        QVERIFY2(manager.collectChannels(), qPrintable(manager.lastError()));
        ChannelModel model(&manager);
        manager.setChannelModel(&model);
        QSortFilterProxyModel proxy;
        proxy.setSourceModel(&model);
        proxy.setDynamicSortFilter(true);
        proxy.sort(0);
        QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
        ChannelRuleValues values;
        values.name = QStringLiteral("Я");
        values.start = QTime(22, 0);
        values.end = QTime(6, 0);
        values.untilDayOffset = 1;
        values.volume = 40;
        values.order = QStringLiteral("sequential");
        QVERIFY(applyChannel(&proxy, 0, values));
        QCOMPARE(changes.size(), 1);
        QCOMPARE(model.index(0, 0).data().toString(), values.name);
        QCOMPARE(model.index(0, 1).data().toString(), QStringLiteral("22:00"));
        QCOMPARE(model.index(0, 2).data().toString(), QStringLiteral("06:00 +1 день"));
        QCOMPARE(model.index(0, 0).data(ChannelModel::UntilDayOffsetRole).toInt(), 1);
        ChannelManager reopened(MUSIC);
        QVERIFY2(reopened.collectChannels(), qPrintable(reopened.lastError()));
        QCOMPARE(reopened.channel(0).untilDayOffset(), 1);
        QCOMPARE(reopened.channel(0).startTime(), QTime(22, 0));
        QCOMPARE(model.index(0, 0).data(ChannelModel::PlaybackOrderRole).toString(), values.order);
        QCOMPARE(model.index(1, 0).data().toString(), QStringLiteral("Б"));
        QVERIFY(!QFileInfo::exists(directory.filePath("media/music/Я")));
        QVERIFY(QFileInfo::exists(directory.filePath("media/music/А")));
        QCOMPARE(reopened.channel(0).storageDirectory(), QStringLiteral("А"));
        QCOMPARE(QDir::cleanPath(reopened.channel(0).mediaManager().getDirMediaFiles().absolutePath()),
                 QDir::cleanPath(directory.filePath("media/music/А")));

        QFile saved(path);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const QByteArray committed = saved.readAll();
        saved.close();
        const auto before = snapshot(model, 0);
        QVERIFY(QFile::rename(path, path + ".previous"));
        QVERIFY(QDir().mkdir(path)); // Deterministic filesystem refusal on Windows and Linux.
        values.volume = 80;
        values.order = QStringLiteral("shuffle_cycle");
        const int proxyRow = proxy.mapFromSource(model.index(0, 0)).row();
        QVERIFY(!applyChannel(&proxy, proxyRow, values));
        QCOMPARE(snapshot(model, 0), before);
        QCOMPARE(changes.size(), 1);
        QVERIFY(!model.lastError().isEmpty());
        QCOMPARE(model.index(0, 0).data(ChannelModel::PlaybackOrderRole).toString(), QStringLiteral("sequential"));
        QFile previous(path + ".previous");
        QVERIFY(previous.open(QIODevice::ReadOnly));
        QCOMPARE(previous.readAll(), committed);
        manager.setChannelModel(nullptr);
    }

    void cancellationDoesNotWrite()
    {
        RecordingModel model;
        model.append(channelRow());
        const auto before = snapshot(model);
        QTimer::singleShot(0, [] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QLineEdit *>(QStringLiteral("channelName"))->setText(QStringLiteral("Вечер"));
            dialog->findChild<QLineEdit *>(QStringLiteral("channelDays"))->setText(QStringLiteral("2–15"));
            dialog->reject();
        });
        QVERIFY(!editChannel(&model, 0));
        QCOMPARE(model.writes, 0);
        QCOMPARE(snapshot(model), before);

        RecordingModel ads;
        ads.append(advertRow());
        const auto adsBefore = snapshot(ads);
        QTimer::singleShot(0, [] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QLineEdit *>(QStringLiteral("advertHours"))->setText(QStringLiteral("8–22"));
            dialog->reject();
        });
        QVERIFY(!editAdvert(&ads, 0));
        QCOMPARE(ads.writes, 0);
        QCOMPARE(snapshot(ads), adsBefore);
    }

    void invalidChannelDraftNeverWrites()
    {
        RecordingModel model;
        model.append(channelRow());
        model.append(channelRow(QStringLiteral("Вечер")));
        ChannelRuleValues values;
        values.name = QStringLiteral("Утро");
        for (const QString &name : {QString(), QStringLiteral("Мягкое утро"), QStringLiteral("СлишкомДлинноеИмяКанала"), QStringLiteral("Вечер"), QStringLiteral("../каталог")}) {
            auto invalid = values;
            invalid.name = name;
            QVERIFY(!applyChannel(&model, 0, invalid));
        }
        for (const QString &days : {QStringLiteral("0"), QStringLiteral("32"), QStringLiteral("12–2"), QStringLiteral("1,,3"), QStringLiteral("1;2")}) {
            auto invalid = values;
            invalid.days = days;
            QVERIFY(!applyChannel(&model, 0, invalid));
        }
        values.months = QStringLiteral("0,13");
        QVERIFY(!applyChannel(&model, 0, values));
        values.months.clear();
        QVERIFY(!applyChannel(&model, 0, values));
        values.months = QStringLiteral("*");
        values.weekdays.clear();
        QVERIFY(!applyChannel(&model, 0, values));
        values.weekdays = QStringLiteral("*");
        values.start = QTime();
        QVERIFY(!applyChannel(&model, 0, values));
        QCOMPARE(model.writes, 0);
    }

    void keyboardChoiceAndCancellation()
    {
        ChannelRuleDialog dialog(ChannelRuleValues{});
        auto *monday = dialog.findChild<QPushButton *>(QStringLiteral("weekday1"));
        const bool selected = monday->isChecked();
        QTest::keyClick(monday, Qt::Key_Return);
        QCOMPARE(monday->isChecked(), !selected);
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        dialog.show();
        auto *cancel = dialog.findChild<QPushButton *>(QStringLiteral("cancelRule"));
        cancel->setFocus();
        QTest::keyClick(cancel, Qt::Key_Return);
        QVERIFY(!dialog.isVisible());
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
    }

    void channelOrderCanBeChosenAndReopened()
    {
        RecordingModel model;
        model.append(channelRow());
        ChannelRuleDialog dialog(ChannelRuleValues{});
        auto *order = dialog.findChild<QComboBox *>(QStringLiteral("channelOrder"));
        QVERIFY(order);
        QCOMPARE(order->currentData().toString(), QStringLiteral("shuffle_cycle"));
        order->setCurrentIndex(order->findData(QStringLiteral("sequential")));
        const QString captureDirectory = qEnvironmentVariable("PLAYBACK_UI_CAPTURE_DIR");
        if (!captureDirectory.isEmpty()) {
            QVERIFY(Restyle::verifiedCyrillicFont());
            QVERIFY(QDir().mkpath(captureDirectory));
            dialog.show();
            QTest::qWait(50);
            QVERIFY(dialog.grab().save(QDir(captureDirectory).filePath(QStringLiteral("channel-order.png"))));
            dialog.hide();
        }
        QVERIFY(applyChannel(&model, 0, dialog.values()));
        QCOMPARE(model.index(0, 0).data(ChannelModel::PlaybackOrderRole).toString(), QStringLiteral("sequential"));
        QTimer::singleShot(0, [] {
            auto *editor = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(editor);
            auto *choice = editor->findChild<QComboBox *>(QStringLiteral("channelOrder"));
            QCOMPARE(choice->currentData().toString(), QStringLiteral("sequential"));
            choice->setCurrentIndex(choice->findData(QStringLiteral("shuffle_cycle")));
            editor->accept();
        });
        QVERIFY(editChannel(&model, 0));
        QCOMPARE(model.index(0, 0).data(ChannelModel::PlaybackOrderRole).toString(), QStringLiteral("shuffle_cycle"));
    }

    void fullDayCanBeChosenReopenedAndReverted()
    {
        ChannelRuleValues initial;
        initial.start = QTime(22, 0); initial.end = QTime(6, 0); initial.untilDayOffset = 1;
        ChannelRuleDialog dialog(initial);
        auto *full = dialog.findChild<QCheckBox *>(QStringLiteral("channelFullDay"));
        auto *next = dialog.findChild<QCheckBox *>(QStringLiteral("channelNextDay"));
        QVERIFY(full && next && next->isChecked());
        full->setChecked(true);
        QCOMPARE(dialog.values().start, QTime(0, 0));
        QCOMPARE(dialog.values().end, QTime(0, 0));
        QCOMPARE(dialog.values().untilDayOffset, 1);
        QVERIFY(!dialog.findChild<QTimeEdit *>(QStringLiteral("channelStart"))->isEnabled());
        RecordingModel model; model.append(channelRow(initial.name));
        QVERIFY(applyChannel(&model, 0, dialog.values()));
        QCOMPARE(model.index(0, 0).data(ChannelModel::UntilDayOffsetRole).toInt(), 1);
        QTimer::singleShot(0, [] {
            auto *editor = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(editor);
            QVERIFY(editor->findChild<QCheckBox *>(QStringLiteral("channelFullDay"))->isChecked());
            editor->accept();
        });
        QVERIFY(editChannel(&model, 0));
        const QString capture = qEnvironmentVariable("PLAYBACK_UI_CAPTURE_DIR");
        if (!capture.isEmpty()) {
            QVERIFY(Restyle::verifiedCyrillicFont());
            QVERIFY(QDir().mkpath(capture));
            dialog.show(); QTest::qWait(50);
            QVERIFY(dialog.grab().save(QDir(capture).filePath(QStringLiteral("channel-full-day.png"))));
            dialog.hide();
        }
        full->setChecked(false);
        QCOMPARE(dialog.values().start, initial.start);
        QCOMPARE(dialog.values().end, initial.end);
        QCOMPARE(dialog.values().untilDayOffset, initial.untilDayOffset);
        QVERIFY(next->isEnabled());
    }

    void completeDocumentEditorValidatesBeforeSaving()
    {
        QFile file(QFINDTESTDATA("../Documentation/schedule-v1/example.new-year.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        ScheduleV1::Document source;
        const QString error = ScheduleV1::parse(file.readAll(), &source);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const QString expired = ScheduleDocumentUi::describe(source.object, QDateTime(QDate(2028, 1, 1), QTime(12, 0)));
        QVERIFY(expired.contains(QStringLiteral("Резервный источник")));
        QVERIFY(expired.contains(QStringLiteral("Тишина")));
        ScheduledDocumentDialog dialog(source.object);
        dialog.findChild<QToolButton *>(QStringLiteral("scheduleDocumentDetails"))->setChecked(true);
        auto *section = dialog.findChild<QComboBox *>(QStringLiteral("scheduleDocumentSection"));
        auto *editor = dialog.findChild<QPlainTextEdit *>(QStringLiteral("scheduleDocumentJson"));
        QVERIFY(section && editor);
        section->setCurrentIndex(6);
        QJsonObject mix = QJsonDocument::fromJson(editor->toPlainText().toUtf8()).object();
        mix.insert("name", QStringLiteral("Праздник через один"));
        editor->setPlainText(QString::fromUtf8(QJsonDocument(mix).toJson()));
        section->setCurrentIndex(3);
        QCOMPARE(dialog.document().value("mixRules").toArray().first().toObject().value("name").toString(), QStringLiteral("Праздник через один"));
        const auto calendarText = editor->toPlainText();
        editor->setPlainText(QStringLiteral("{broken"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("scheduleDocumentError"))->text().isEmpty());
        editor->setPlainText(calendarText);
        auto *at = dialog.findChild<QDateTimeEdit *>(QStringLiteral("scheduleDocumentAt"));
        at->setDateTime(QDateTime(QDate(2026, 12, 31), QTime(12, 0)));
        section->setCurrentIndex(6);
        dialog.findChild<QPushButton *>(QStringLiteral("scheduleDocumentCheck"))->click();
        QVERIFY(dialog.findChild<QPlainTextEdit *>(QStringLiteral("scheduleDocumentPreview"))->toPlainText().contains(QStringLiteral("Чередование")));
        const QString capture = qEnvironmentVariable("PLAYBACK_UI_CAPTURE_DIR");
        if (!capture.isEmpty()) {
            QVERIFY(Restyle::verifiedCyrillicFont());
            QVERIFY(QDir().mkpath(capture));
            dialog.show(); QTest::qWait(50);
            QVERIFY(dialog.grab().save(QDir(capture).filePath(QStringLiteral("schedule-project.png"))));
            dialog.hide();
        }
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }

    void holidayFormAddsInclusiveRotationToExistingSchedule()
    {
        QFile file(QFINDTESTDATA("../Documentation/schedule-v1/example.new-year.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto source = QJsonDocument::fromJson(file.readAll()).object();
        source.insert("mixRules", QJsonArray{});
        source.insert("requiredCapabilities", QJsonArray::fromStringList(ScheduleV1::requiredCapabilities(source)));
        ScheduledDocumentDialog dialog(source);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Настройки расписания"));
        QVERIFY(dialog.findChild<QWidget *>(QStringLiteral("scheduleDocumentDetailsPanel"))->isHidden());
        auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("scheduleHolidayName"));
        auto *playlist = dialog.findChild<QComboBox *>(QStringLiteral("scheduleHolidayPlaylist"));
        auto *from = dialog.findChild<QDateEdit *>(QStringLiteral("scheduleHolidayFrom"));
        auto *until = dialog.findChild<QDateEdit *>(QStringLiteral("scheduleHolidayUntil"));
        auto *add = dialog.findChild<QPushButton *>(QStringLiteral("scheduleHolidayAdd"));
        auto *error = dialog.findChild<QLabel *>(QStringLiteral("scheduleDocumentError"));
        QVERIFY(name && playlist && from && until && add && error);
        name->setText(QStringLiteral("Новогодняя музыка"));
        playlist->setCurrentIndex(playlist->count() - 1);
        from->setDate(QDate(2026, 12, 15));
        until->setDate(QDate(2027, 1, 15));
        add->click();
        QVERIFY2(error->text().isEmpty(), qPrintable(error->text()));
        const auto result = dialog.document();
        QCOMPARE(result.value("baseRules"), source.value("baseRules"));
        QCOMPARE(result.value("dayTemplates"), source.value("dayTemplates"));
        QCOMPARE(result.value("eventRules"), source.value("eventRules"));
        QCOMPARE(result.value("mixRules").toArray().size(), 1);
        const auto rule = result.value("mixRules").toArray().first().toObject();
        QCOMPARE(rule.value("pattern").toArray().size(), 2);
        QCOMPARE(rule.value("pattern").toArray().first().toObject().value("type").toString(), QStringLiteral("active_base"));
        QCOMPARE(rule.value("pattern").toArray().last().toObject().value("playlistId").toString(), playlist->currentData().toString());
        ScheduleV1::Document compiled;
        const auto validationError = ScheduleV1::decode(result, &compiled);
        QVERIFY2(validationError.isEmpty(), qPrintable(validationError));
        const QTimeZone zone(result.value("timeZone").toString().toUtf8());
        const auto evaluate = [&](const QDate &date) {
            return ScheduleV1::evaluate(compiled, QDateTime(date, QTime(12, 0), zone));
        };
        QVERIFY(evaluate(QDate(2026, 12, 14)).mixRuleId.isEmpty());
        QCOMPARE(evaluate(QDate(2026, 12, 15)).mixRuleId, rule.value("id").toString());
        QCOMPARE(evaluate(QDate(2027, 1, 15)).mixRuleId, rule.value("id").toString());
        QVERIFY(evaluate(QDate(2027, 1, 16)).mixRuleId.isEmpty());
        const QString capture = qEnvironmentVariable("PLAYBACK_UI_CAPTURE_DIR");
        if (!capture.isEmpty()) {
            QVERIFY(Restyle::verifiedCyrillicFont());
            QVERIFY(QDir().mkpath(capture));
            dialog.show(); QTest::qWait(50);
            QVERIFY(dialog.grab().save(QDir(capture).filePath(QStringLiteral("schedule-settings.png"))));
            dialog.hide();
        }
        add->click(); // An overlapping rotation must not silently replace the existing one.
        QVERIFY(!error->text().isEmpty());
        QCOMPARE(dialog.document().value("mixRules"), result.value("mixRules"));
        dialog.findChild<QPushButton *>(QStringLiteral("scheduleHolidayRemove"))->click();
        QVERIFY(dialog.document().value("mixRules").toArray().isEmpty());
        from->setDate(QDate(2026, 12, 20));
        until->setDate(QDate(2026, 12, 19));
        add->click();
        QVERIFY(error->text().contains(QStringLiteral("окончания")));
        QVERIFY(dialog.document().value("mixRules").toArray().isEmpty());
        until->setDate(QDate(2027, 2, 1));
        add->click();
        QVERIFY(error->text().contains(QStringLiteral("срок расписания")));
        QVERIFY(dialog.document().value("mixRules").toArray().isEmpty());
    }

    void channelPickerUsesCronNumbers()
    {
        ChannelRuleValues initial;
        initial.name = QStringLiteral("Утро");
        initial.weekdays = QStringLiteral("0,1");
        initial.months = QStringLiteral("1,12");
        ChannelRuleDialog dialog(initial);
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("weekday0"))->isChecked());
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("weekday1"))->isChecked());
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("weekday2"))->isChecked());
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("month1"))->isChecked());
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("month12"))->isChecked());
        QCOMPARE(dialog.values().weekdays, QStringLiteral("0,1"));
        QCOMPARE(dialog.values().months, QStringLiteral("1,12"));
        dialog.findChild<QLineEdit *>(QStringLiteral("channelDays"))->setText(QStringLiteral("1–3, 10"));
        RecordingModel model;
        model.append(channelRow());
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(applyChannel(&model, 0, dialog.values()));
        QCOMPARE(model.index(0, 4).data().toString(), QStringLiteral("1,2,3,10"));
        QCOMPARE(model.index(0, 3).data().toString(), QStringLiteral("0,1"));
        QCOMPARE(model.index(0, 5).data().toString(), QStringLiteral("1,12"));
    }

    void failedSaveRestoresPreviousFields()
    {
        RecordingModel model;
        model.append(channelRow());
        const auto before = snapshot(model);
        model.refuseColumn = 5;
        ChannelRuleValues values;
        values.name = QStringLiteral("Вечер");
        values.start = QTime(16, 0);
        values.end = QTime(20, 0);
        values.days = QStringLiteral("1–10");
        values.months = QStringLiteral("10,11");
        QVERIFY(!applyChannel(&model, 0, values));
        QVERIFY(model.refused);
        QCOMPARE(snapshot(model), before);
    }

    void proxyRenameKeepsOtherFieldsOnSameChannel()
    {
        RecordingModel model;
        model.append(channelRow(QStringLiteral("А")));
        model.append(channelRow(QStringLiteral("Б")));
        QSortFilterProxyModel proxy;
        proxy.setSourceModel(&model);
        proxy.setDynamicSortFilter(true);
        proxy.sort(0);
        ChannelRuleValues values;
        values.name = QStringLiteral("Я");
        values.start = QTime(18, 0);
        values.volume = 42;
        QVERIFY(applyChannel(&proxy, 0, values));
        QCOMPARE(model.index(0, 0).data().toString(), QStringLiteral("Я"));
        QCOMPARE(model.index(0, 1).data().toTime(), QTime(18, 0));
        QCOMPARE(model.index(0, 6).data().toInt(), 42);
        QCOMPARE(snapshot(model, 1), channelRow(QStringLiteral("Б")));
    }

    void advertisingModesPreserveFormat()
    {
        RecordingModel model;
        model.append(advertRow());
        AdvertRuleValues values;
        values.fileName = QStringLiteral("Ролик.mp3");
        values.start = QDate(2026, 10, 1);
        values.end = QDate(2026, 10, 31);
        AdvertRuleDialog dialog(values);
        dialog.findChild<QLineEdit *>(QStringLiteral("advertHours"))->setText(QStringLiteral("22–2"));
        dialog.findChild<QLineEdit *>(QStringLiteral("advertMinutes"))->setText(QStringLiteral("0, 15, 30–32"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(applyAdvert(&model, 0, dialog.values()));
        QCOMPARE(model.index(0, 1).data().toString(), QStringLiteral("0,1,2,22,23"));
        QCOMPARE(model.index(0, 2).data().toString(), QStringLiteral("00m,15m,30m,31m,32m"));
        dialog.findChild<QComboBox *>(QStringLiteral("advertMode"))->setCurrentIndex(1);
        dialog.findChild<QSpinBox *>(QStringLiteral("advertFrequency"))->setValue(3);
        QVERIFY(applyAdvert(&model, 0, dialog.values()));
        QCOMPARE(model.index(0, 2).data().toString(), QStringLiteral("3"));
        dialog.findChild<QComboBox *>(QStringLiteral("advertMode"))->setCurrentIndex(2);
        QVERIFY(applyAdvert(&model, 0, dialog.values()));
        QCOMPARE(model.index(0, 2).data().toString(), QStringLiteral("*"));
    }

    void invalidAdvertisingAndReadOnlyNeverWrite()
    {
        RecordingModel model;
        model.append(advertRow());
        AdvertRuleValues values;
        values.fileName = QStringLiteral("Ролик.mp3");
        for (const QString &minutes : {QStringLiteral("0"), QStringLiteral("6"), QStringLiteral("20"), QStringLiteral("21"), QStringLiteral("60m"), QStringLiteral("00m,30"), QStringLiteral("2,3")}) {
            values.minutes = minutes;
            QVERIFY(!applyAdvert(&model, 0, values));
        }
        values.minutes = QStringLiteral("00m");
        values.hours = QStringLiteral("24");
        QVERIFY(!applyAdvert(&model, 0, values));
        values.hours = QStringLiteral("*");
        values.end = values.start.addDays(-1);
        QVERIFY(!applyAdvert(&model, 0, values));
        values.end = values.start;
        model.item(0, 1)->setEditable(false);
        QVERIFY(!applyAdvert(&model, 0, values));
        QCOMPARE(model.writes, 0);
    }

    void invalidSavedValuesRequireCorrection()
    {
        AdvertRuleValues initial;
        initial.fileName = QStringLiteral("Ролик.mp3");
        initial.minutes = QStringLiteral("00m,30");
        AdvertRuleDialog dialog(initial);
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("formError"))->text().isEmpty());
        dialog.findChild<QLineEdit *>(QStringLiteral("advertMinutes"))->setText(QStringLiteral("0,15"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }

    void themeSwitchRetainsDraft()
    {
        ChannelRuleDialog dialog(ChannelRuleValues{});
        auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("channelName"));
        name->setText(QStringLiteral("Вечер"));
        dialog.findChild<QPushButton *>(QStringLiteral("weekday0"))->click();
        const auto before = dialog.values();
        Restyle::apply(QStringLiteral("tide"), QStringLiteral("dark"));
        QApplication::processEvents();
        QCOMPARE(dialog.values().name, before.name);
        QCOMPARE(dialog.values().weekdays, before.weekdays);
        Restyle::apply(QStringLiteral("tide-relief"), QStringLiteral("denim"));
    }
};

QTEST_MAIN(RuleEditorsTest)
#include "tst_ruleeditors.moc"
