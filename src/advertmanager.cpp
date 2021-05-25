#include "advertmanager.h"
#include "boxlog.h"

#include "trackfullinfo.h"
#include "advertcsvgenerator.h"
#include "informer.h"
#include "stationmanager.h"
#include "trialmessagebox.h"

#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QDir>
#include <QScopedPointer>


AdvertManager::AdvertManager(QObject *parent) :
    QObject(parent),
    mDateFormat(LAMP_DATE_FORMAT)
{
    mFileAdvertTask = mPathData.advertTask;
    mFileAdverTable = /*qApp->applicationDirPath() + QDir::separator() +*/ mPathData.advertTableFile;
    mFileAdvertView = mPathData.advertViewFile;
    collectAdvert();
}

int AdvertManager::column()
{
    /// Количество столбцов
    ///1 Название
    ///2 Часы
    ///3 Минуты
    ///4 Дни недели
    ///5 Начало
    ///6 Конец
    ///7 Громкость
    return 7;
}

int AdvertManager::count()
{
    return mAdvertDataList.size();
}

void AdvertManager::collectAdvert()
{
    emit beginCollect();
    mAdvertDataList.clear();

    if( STATION_NETWORK == StationManager::Instance().type() )
    {
        QFile file( mFileAdvertView );
        if( !file.exists() ) {
            QFile(mFileAdverTable).copy(file.fileName());
        }
    }

//    QFile file( mFileAdverTable );
    QFile file( mFileAdvertView );
    if ( !file.open(QIODevice::ReadOnly | QIODevice::Text) ) {
        BoxLog() << "Невозможно открыть файл:" << file.fileName();
    } else {
        QTextStream stream(&file);
        while( !stream.atEnd() ) {
            QString line = stream.readLine();
            mAdvertDataList.append( parseAdvert(line) );
        }
        file.close();
    }
    emit endCollect();
}

AdvertData AdvertManager::parseAdvert(const QString &line)
{
    AdvertData data;
    QStringList list = line.split(';');
    data.setName(list[0]);
    data.setHours(list[1]);
    data.setMinuts(list[2]);
    data.setDays(list[3]);
    data.setStartDate( QDate::fromString( list[4], mDateFormat) );
    data.setEndDate( QDate::fromString( list[5], mDateFormat) );
    data.setVolume(list[6].toInt());
    return data;
}


bool AdvertManager::saveAdvert()
{
    QFile file(mFileAdvertView);
    if( !file.open( QFile::WriteOnly | QFile::Text ) ) {
        BoxLog() << "Невозможно открыть файл:" << file.fileName();
        return false;
    }

    QTextStream stream(&file);

    int size = mAdvertDataList.size();
    for(int i = 0; i < size; ++i) {
        stream << mAdvertDataList[i].name()
               << ";" << mAdvertDataList[i].hours()
               << ";" << mAdvertDataList[i].minuts()
               << ";" << mAdvertDataList[i].days()
               << ";" << mAdvertDataList[i].startDate().toString(mDateFormat)
               << ";" << mAdvertDataList[i].endDate().toString(mDateFormat)
               << ";" << QString::number( mAdvertDataList[i].volume() )
               << "\n";
    }
    file.close();

    /// Генерация рекламы
    generateAdvert();

    return true;
}

bool AdvertManager::addAdvert(const QString &name)
{
    if( StationManager::Instance().trial() ) {
        if( mAdvertDataList.size() > 1 ) {
            TrialMessageBox();
            return false;
        }
    }

    QFile file( mFileAdvertView );
    if( !file.open( QFile::Append | QFile::Text ) ) {
        BoxLog() << "Невозможно открыть файл:" << file.fileName();
        return false;
    }

    QDate currentDate = QDate::currentDate();
    QString stringCurrentDate = currentDate.toString(mDateFormat);
    QString line = name + tr(";*;00m;0,1,2,3,4,5,6;%1;%1;100\n").arg(stringCurrentDate);
    QTextStream stream(&file);
    stream << line;
    file.close();

    emit beginCollect();
    mAdvertDataList.append(parseAdvert(line));
    emit endCollect();

    generateAdvert();

return true;
}

bool AdvertManager::delAdvert(int num)
{
    if( num < 0 || num >= mAdvertDataList.size() )
        return false;

    AdvertData temp = mAdvertDataList.takeAt(num);
    if( saveAdvert() ) {
        emit beginCollect();
        emit endCollect();
    } else {
        mAdvertDataList.insert(num, temp);
        return false;
    }

    return true;
}

AdvertData& AdvertManager::advert(int num)
{
    int id;
    if( num <= 0 )
        id = 0;
    else if( num >= mAdvertDataList.size() )
        id = mAdvertDataList.size() - 1;
    else
        id = num;

return mAdvertDataList[id];
}

const QList<AdvertData>& AdvertManager::advertDataList()
{
    return mAdvertDataList;
}


bool AdvertManager::generateAdvert()
{
    TracksFullInfo trackList;
    const QString dateFormat = "yyyy-MM-dd";
    for( auto it = mAdvertDataList.begin(); it != mAdvertDataList.end(); ++it ) {
//        QStringList minuts = it->minuts().split(',');
//        for( auto i = 0; i < minuts.size(); ++i ) {
            if( "*" == it->minuts() )
                continue;
            TrackFullInfo track(it->name(), /*minuts[i]*/it->minuts(), it->hours(), it->days(), it->startDate().toString(dateFormat), it->endDate().toString(dateFormat), it->name(), QString::number( it->volume()) );
            trackList.push_back(track);
//        }
    }

    if( QFile::exists(mFileAdverTable) ) {
        if( !QFile::remove(mFileAdverTable) ) {
            Informer::Instance().infoEvent("Невозможно обновить файл расписания!");
            BoxLog() << "Cannot delete file:" << mFileAdverTable << "!";
            return false;
        }
    }

    if( !trackList.size() ) {
        Informer::Instance().infoEvent("Отсутствует расписание рекламы, сформируйте расписание!");
        BoxLog() << "Отсутствует расписание рекламы!";
        return false;
    }

    QScopedPointer<AdvertCsvGenerator> generator(new AdvertCsvGenerator);

    if( !generator->makeAdvert(trackList) ) {
        Informer::Instance().infoEvent("Нет доступа к файлу расписания!");
        BoxLog() << "Cannot open file:" << mFileAdverTable << "!";
        return false;
    }


    if( !generator->file().copy(mFileAdverTable) ) {
        Informer::Instance().infoEvent("Невозможно обновить файл расписания!");
        BoxLog() << "Cannot copy file:" << mFileAdverTable << "!";
        return false;
    }


return true;
}
