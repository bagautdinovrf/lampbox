#include <QComboBox>
#include <QDataStream>
#include <QFile>
#include <QLocale>
#include <QTest>
#include <QTemporaryDir>

#include "report.h"
#include <audioproperties.h>
#include <fileref.h>
#include <tag.h>

class FoundationTest final : public QObject
{
    Q_OBJECT

private slots:
    void reportMonthNames()
    {
        Report report;
        const auto *months = report.findChild<QComboBox *>("cbMonth");
        QVERIFY(months);
        QCOMPARE(months->count(), 12);
        QCOMPARE(months->itemText(0), QLocale::system().monthName(1, QLocale::LongFormat));
        QCOMPARE(months->itemText(11), QLocale::system().monthName(12, QLocale::LongFormat));
    }

    void resources()
    {
        QVERIFY(QFile::exists(":/player/icons/app.ico"));
        QVERIFY(QFile::exists(":/restyle/icons/music.svg"));
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
        stream << quint32(12036);
        stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1)
               << quint32(8000) << quint32(8000) << quint16(1) << quint16(8);
        stream.writeRawData("data", 4);
        stream << quint32(12000);
        const QByteArray silence(12000, char(128));
        QCOMPARE(stream.writeRawData(silence.constData(), silence.size()), 12000);
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
            QCOMPARE(media.audioProperties()->lengthInMilliseconds(), 1500);
            QVERIFY(media.tag());
            media.tag()->setTitle(TagLib::String("Музыка", TagLib::String::UTF8));
            media.tag()->setArtist(TagLib::String("Исполнитель", TagLib::String::UTF8));
            media.tag()->setAlbum(TagLib::String("Альбом", TagLib::String::UTF8));
            media.tag()->setGenre(TagLib::String("Жанр", TagLib::String::UTF8));
            media.tag()->setYear(2026);
            QVERIFY(media.save());
        }
        TagLib::FileRef saved(fileName.data());
        QVERIFY(!saved.isNull());
        QVERIFY(saved.tag());
        QCOMPARE(QString::fromStdWString(saved.tag()->title().toWString()),
                 QString::fromUtf8("Музыка"));
        QCOMPARE(QString::fromStdWString(saved.tag()->artist().toWString()),
                 QString::fromUtf8("Исполнитель"));
        QCOMPARE(QString::fromStdWString(saved.tag()->album().toWString()),
                 QString::fromUtf8("Альбом"));
        QCOMPARE(QString::fromStdWString(saved.tag()->genre().toWString()),
                 QString::fromUtf8("Жанр"));
        QCOMPARE(saved.tag()->year(), 2026U);
        QVERIFY(saved.audioProperties());
        QCOMPARE(saved.audioProperties()->lengthInSeconds(), 1);
        QCOMPARE(saved.audioProperties()->lengthInMilliseconds(), 1500);
    }

};

QTEST_MAIN(FoundationTest)
#include "tst_foundation.moc"
