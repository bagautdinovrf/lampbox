#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>

#include "report.h"
#include "restyletheme.h"
#include "settings.h"
#include "settingsdialog.h"

namespace {
bool writeReport(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(content) == content.size();
}
}

class SecondaryScreensTest final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(mDirectory.isValid());
        Restyle::install(*qApp);
    }

    void init()
    {
        mSettingsFile = mDirectory.filePath(QString::fromLatin1(QTest::currentTestFunction()) + ".ini");
        qApp->setProperty("restylePreviewSettings", mSettingsFile);
    }

    void cleanup()
    {
        qApp->setProperty("restylePreviewSettings", QVariant());
    }

    void formatActionsPersistAllConfiguredKeys()
    {
        // Existing non-default keys must remain controllable after the restyle.
        Settings initial;
        initial.writeFileFormatAudioValue(QStringLiteral("opus"), false);
        initial.writeFileFormatVideoValue(QStringLiteral("mov"), false);
        Settings configured;
        SettingsDialog page(nullptr, Qt::Widget);
        auto *audio = page.findChild<QListWidget *>(QStringLiteral("lw_AudioFileType"));
        auto *video = page.findChild<QListWidget *>(QStringLiteral("lw_VideoFileType"));
        auto *all = page.findChild<QPushButton *>(QStringLiteral("pb_selectAll"));
        auto *none = page.findChild<QPushButton *>(QStringLiteral("pb_deselectAll"));
        QVERIFY(audio && video && all && none);
        QCOMPARE(audio->count(), configured.fileFormatsAudio().size());
        QCOMPARE(video->count(), configured.fileFormatsVideo().size());
        QSignalSpy audioChanged(&page, &SettingsDialog::fileFormatsAudio);
        QSignalSpy videoChanged(&page, &SettingsDialog::fileFormatsVideo);
        all->click();
        QCOMPARE(audioChanged.count(), 1);
        QCOMPARE(videoChanged.count(), 1);
        Settings checked;
        for (bool value : checked.fileFormatsAudio()) QVERIFY(value);
        for (bool value : checked.fileFormatsVideo()) QVERIFY(value);
        none->click();
        QCOMPARE(audioChanged.count(), 2);
        QCOMPARE(videoChanged.count(), 2);
        Settings unchecked;
        for (bool value : unchecked.fileFormatsAudio()) QVERIFY(!value);
        for (bool value : unchecked.fileFormatsVideo()) QVERIFY(!value);
        audio->item(0)->setCheckState(Qt::Checked);
        Settings edited;
        QVERIFY(edited.fileFormatsAudio().value(audio->item(0)->text()));
    }

    void themeChangeKeepsFormatChoice()
    {
        SettingsDialog page(nullptr, Qt::Widget);
        auto *audio = page.findChild<QListWidget *>(QStringLiteral("lw_AudioFileType"));
        QVERIFY(audio && audio->count());
        audio->setCurrentRow(audio->count() - 1);
        auto *item = audio->currentItem();
        item->setCheckState(Qt::Checked);
        Restyle::apply(QStringLiteral("tide"), QStringLiteral("dark"));
        QCOMPARE(audio->currentItem(), item);
        QCOMPARE(item->checkState(), Qt::Checked);
        QVERIFY(Settings().fileFormatsAudio().value(item->text()));
        auto *summary = page.findChild<QLabel *>(QStringLiteral("appearanceSummary"));
        QVERIFY(summary);
        QCOMPARE(summary->text(), QStringLiteral("Сейчас: Оригинал · Тёмная"));
        Restyle::apply(QStringLiteral("tide-relief"), QStringLiteral("dark"));
        QCOMPARE(summary->text(), QStringLiteral("Сейчас: Рельеф · Тёмная"));
        Restyle::apply(QStringLiteral("tide-relief"), QStringLiteral("denim"));
    }

    void formatCardsFitThreeColumnsWithoutHidingChoices()
    {
        QWidget host;
        host.resize(1440, 732);
        SettingsDialog page(&host);
        page.setWindowFlags(Qt::Widget);
        page.setGeometry(host.rect());
        host.show();
        page.show();
        for (const auto &name : {QStringLiteral("lw_AudioFileType"), QStringLiteral("lw_VideoFileType")}) {
            auto *list = page.findChild<QListWidget *>(name);
            QVERIFY(list && list->count() >= 3);
            QTRY_COMPARE(list->visualItemRect(list->item(2)).top(), list->visualItemRect(list->item(0)).top());
            for (int i = 0; i < list->count(); ++i)
                QVERIFY2(list->viewport()->rect().contains(list->visualItemRect(list->item(i))),
                         qPrintable(QStringLiteral("Format %1 is clipped").arg(list->item(i)->text())));
        }
    }

    void reportFailureNeverLeavesPreviousPeriodVisible()
    {
        Report report;
        report.setReportDirectory(mDirectory.path());
        auto *month = report.findChild<QComboBox *>(QStringLiteral("cbMonth"));
        auto *year = report.findChild<QDateEdit *>(QStringLiteral("yearEdit"));
        auto *table = report.findChild<QTableView *>(QStringLiteral("tableView"));
        auto *empty = report.findChild<QLabel *>(QStringLiteral("reportEmptyTitle"));
        QVERIFY(month && year && table && empty);
        year->setDate(QDate(2026, 1, 1));
        month->setCurrentIndex(9);
        const QString october = mDirectory.filePath(QStringLiteral("1026.csv"));
        QVERIFY(writeReport(october, QStringLiteral("Маяк — За окном;64\nAster — Новый день;47;legacy\n").toUtf8()));
        report.Generate();
        QVERIFY(table->model());
        QCOMPARE(table->model()->rowCount(), 2);
        QCOMPARE(table->model()->index(0, 0).data().toString(), QStringLiteral("Маяк — За окном"));
        QCOMPARE(table->model()->index(1, 1).data().toInt(), 47);
        // Repeated loads replace the model contents rather than appending.
        report.Generate();
        QCOMPARE(table->model()->rowCount(), 2);
        QCOMPARE(report.findChildren<CompositionsList *>().size(), 1);

        month->setCurrentIndex(10);
        report.Generate();
        QVERIFY(!table->model());
        QCOMPARE(empty->text(), QStringLiteral("За этот период отчётов нет"));
        const QString november = mDirectory.filePath(QStringLiteral("1126.csv"));
        QVERIFY(writeReport(november, "malformed row\n"));
        report.Generate();
        QVERIFY(!table->model());
        QCOMPARE(empty->text(), QStringLiteral("Не удалось открыть отчёт"));
        QVERIFY(writeReport(november, "track;-1\n"));
        report.Generate();
        QVERIFY(!table->model());
        QCOMPARE(empty->text(), QStringLiteral("Не удалось открыть отчёт"));
        QVERIFY(writeReport(november, {}));
        report.Generate();
        QVERIFY(!table->model());
        QCOMPARE(empty->text(), QStringLiteral("В отчёте нет записей"));
    }

private:
    QTemporaryDir mDirectory;
    QString mSettingsFile;
};

QTEST_MAIN(SecondaryScreensTest)
#include "tst_secondary.moc"
