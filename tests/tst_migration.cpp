#include <QComboBox>
#include <QDataStream>
#include <QFile>
#include <QLineEdit>
#include <QLocale>
#include <QStandardItemModel>
#include <QTest>
#include <QTemporaryDir>
#include <QValidator>

#include <memory>
#include <algorithm>
#include <utility>

#include "simplelineeditdelegate.h"
#include "report.h"
#include "hours.h"
#include "balance.h"
#include "qcrongenerator.h"
#include "delegatemonthedit.h"
#include "delegatedatetimeperiod.h"
#include <audioproperties.h>
#include <fileref.h>
#include <tag.h>

static_assert(std::to_underlying(Qt::Sunday) == 7);

class MigrationTest final : public QObject
{
    Q_OBJECT

private slots:
    void channelNames_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("latin") << QString("Music_12") << true;
        QTest::newRow("cyrillic") << QString::fromUtf8("Музыка_12") << true;
        QTest::newRow("spaces") << QString::fromUtf8("Моя музыка") << false;
        QTest::newRow("path") << QString("../music") << false;
    }

    void channelNames()
    {
        QFETCH(QString, text);
        QFETCH(bool, accepted);
        lampbox::SimpleLineEditDelegate delegate;
        QWidget parent;
        std::unique_ptr<QWidget> editor(delegate.createEditor(&parent, {}, {}));
        const auto *lineEdit = qobject_cast<QLineEdit *>(editor.get());
        QVERIFY(lineEdit);
        QVERIFY(lineEdit->validator());
        int position = 0;
        const auto state = lineEdit->validator()->validate(text, position);
        QCOMPARE(state == QValidator::Acceptable, accepted);
    }

    void reportMonthNames()
    {
        Report report;
        const auto *months = report.findChild<QComboBox *>("cbMonth");
        QVERIFY(months);
        QCOMPARE(months->count(), 12);
        QCOMPARE(months->itemText(0), QLocale::system().monthName(1, QLocale::LongFormat));
        QCOMPARE(months->itemText(11), QLocale::system().monthName(12, QLocale::LongFormat));
    }

    void monthDelegateLocale()
    {
        lampproject::delegate::DelegateMonthEdit delegate;
        const QLocale russian(QLocale::Russian);
        const QString display = delegate.displayText(QString("1,12"), russian);
        QVERIFY(display.contains(russian.monthName(1, QLocale::ShortFormat)));
        QVERIFY(display.contains(russian.monthName(12, QLocale::ShortFormat)));
    }

    void scheduleRanges()
    {
        using namespace lampproject::delegate;
        QStandardItemModel model(2, 1);
        const QModelIndex target = model.index(0, 0);
        const QModelIndex untouched = model.index(1, 0);
        // The model stores expanded values; setEditorData groups them for editing.
        model.setData(target, QString("1,2,3"));
        model.setData(untouched, QString("unchanged"));
        DelegateDateTimePeriodEdit delegate(hour);
        QWidget parent;
        std::unique_ptr<QWidget> editor(delegate.createEditor(&parent, {}, target));
        QVERIFY(editor);
        delegate.setEditorData(editor.get(), target);
        QCOMPARE(model.data(target).toString(), QString("1,2,3"));
        delegate.setModelData(editor.get(), &model, target);
        QCOMPARE(model.data(target).toString(), QString("1,2,3"));
        QCOMPARE(model.data(untouched).toString(), QString("unchanged"));

        // Enter a wrapping range directly, as the editor does before model commit.
        std::unique_ptr<QWidget> wrappingEditor(delegate.createEditor(&parent, {}, target));
        auto *periodEditor = qobject_cast<DelegateDateTimePeriod *>(wrappingEditor.get());
        QVERIFY(periodEditor);
        QVERIFY(periodEditor->setValue(QString("22-2")));
        QCOMPARE(model.data(target).toString(), QString("1,2,3"));
        delegate.setModelData(periodEditor, &model, target);
        QCOMPARE(model.data(target).toString(), QString("0,1,2,22,23"));
        QCOMPARE(model.data(untouched).toString(), QString("unchanged"));
    }

    void resources()
    {
        QVERIFY(QFile::exists(":/player/music.png"));
        QVERIFY(QFile::exists(":/ico/res/period.png"));
    }

    void hoursAndOwnership()
    {
        Hours hours({{"8", 1}, {"9", 0}, {"10", 3}});
        QCOMPARE(hours.GetHoursString(), QString("8,10"));
        QCOMPARE(hours.GetFrequency(), 3);
        QCronGenerator generator(nullptr);
        QString error;
        QVERIFY(!generator.Init({}, error, false));
        QVERIFY(!error.isEmpty());
    }

    void unicodeMediaMetadata()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QString::fromUtf8("Музыка.wav"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData("RIFF", 4);
        stream << quint32(8036);
        stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1)
               << quint32(8000) << quint32(8000) << quint16(1) << quint16(8);
        stream.writeRawData("data", 4);
        stream << quint32(8000);
        const QByteArray silence(8000, char(128));
        QCOMPARE(stream.writeRawData(silence.constData(), silence.size()), 8000);
        file.close();

#ifdef Q_OS_WIN
        const auto fileName = path.toStdWString();
#else
        const auto fileName = QFile::encodeName(path);
#endif
        {
            TagLib::FileRef media(fileName.data());
            QVERIFY(!media.isNull());
            QVERIFY(media.audioProperties());
            QCOMPARE(media.audioProperties()->lengthInSeconds(), 1);
            QVERIFY(media.tag());
            media.tag()->setTitle(TagLib::String("Музыка", TagLib::String::UTF8));
            QVERIFY(media.save());
        }
        TagLib::FileRef saved(fileName.data());
        QVERIFY(!saved.isNull());
        QCOMPARE(QString::fromStdWString(saved.tag()->title().toWString()),
                 QString::fromUtf8("Музыка"));
    }

    void balancedTrackCounts()
    {
        Balance balance(false);
        bool success = false;
        const auto tracks = balance.MakeBalanced({{"A", 5}, {"B", 3}, {"C", 1}}, success);
        QVERIFY(success);
        QCOMPARE(tracks.size(), std::size_t(9));
        QCOMPARE(std::ranges::count(tracks, QString("A")), 5);
        QCOMPARE(std::ranges::count(tracks, QString("B")), 3);
        QCOMPARE(std::ranges::count(tracks, QString("C")), 1);
    }
};

QTEST_MAIN(MigrationTest)
#include "tst_migration.moc"
