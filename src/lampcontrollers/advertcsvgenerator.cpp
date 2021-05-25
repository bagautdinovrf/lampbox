//#include "trackfullinfo.h"
#include "advertcsvgenerator.h"
#include "controller.h"
#include "trackdistributor.h"

#include <QScopedPointer>
#include <vector>

using std::vector;
AdvertCsvGenerator::AdvertCsvGenerator(QObject *parent) :
    QObject(parent)
{
    //
}


bool AdvertCsvGenerator::makeAdvert(TracksFullInfo &tracks)
{
    if( !tracks.size() )
        return false;

    QScopedPointer<TrackDistributor> _distributor(new TrackDistributor(tracks));
    _distributor->distribute();

    QString advertResult;
    for(size_t i = 0; i < tracks.size(); ++i) {
        QString startDate = QDate::fromString(tracks[i].startTime(), "yyyy-MM-dd").toString("dd.MM.yyyy");
        QString endDate   = QDate::fromString(tracks[i].endTime(),   "yyyy-MM-dd").toString("dd.MM.yyyy");
        advertResult.append(QString("%1;%2;%3;%4;%5;%6;%7;%8\n").
                     arg( tracks[i].GetFileName(),
                          tracks[i].GetHours(),
                          tracks[i].minuts(),
                          tracks[i].GetDayOfWeek(),
                          startDate,
                          endDate,
                          tracks[i].volume(),
                          tracks[i].order() ) );
    }

    return saveFile(advertResult);
}

bool AdvertCsvGenerator::saveFile(const QString &text)
{
    if( mTempFile.open() ) {
        QTextStream out(&mTempFile);
        out << text;
        mTempFile.close();
        return true;
    } else {
        return false;
    }
}

QTemporaryFile &AdvertCsvGenerator::file()
{
    return mTempFile;
}
