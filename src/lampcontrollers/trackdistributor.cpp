#include "trackdistributor.h"

#include <QRandomGenerator>
#include <utility>

TrackDistributor::TrackDistributor(TracksFullInfo &tracks, QObject *parent) :
    QObject(parent),
    mTracks(tracks)
{
}


TracksFullInfo& TrackDistributor::distribute()
{
    if( mTracks.empty() )
        return mTracks;

    counting();

    if(mTrackList.isEmpty())
        return mTracks;

    ranking();

    return mTracks;
}


void TrackDistributor::counting()
{
    for (std::size_t i = 0; i < mTracks.size(); ++i) {
        if( mTracks[i].GetFrequency().contains('m') ) {
            mTracks[i].AddMinute( mTracks[i].GetFrequency() );
            continue;
        }
        else {
            const int frequency = mTracks[i].GetFrequency().toInt();
            // The editor permits one to five plays per hour.
            if (frequency >= 1 && frequency <= 5)
                mTrackList.emplaceBack(i, frequency);
        }
    }
}


void TrackDistributor::ranking()
{
    for (const auto &[index, frequency] : std::as_const(mTrackList)) {
        const int step = 60 / frequency;
        int minute = QRandomGenerator::global()->bounded(step - 1);
        for (int j = 0; j < frequency; ++j) {
            mTracks[index].AddMinute(QString::number(minute) + 'm');
            minute += step;
        }
    }
}
