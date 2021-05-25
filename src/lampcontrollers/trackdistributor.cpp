#include "trackdistributor.h"

#include <vector>
#include <time.h>

TrackDistributor::TrackDistributor(TracksFullInfo &tracks, QObject *parent) :
    QObject(parent),
    mTracks(tracks)
{
    srand(time(NULL));
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
    for( uint i = 0; i < mTracks.size(); ++i ) {
        if( mTracks[i].GetFrequency().contains('m') ) {
            mTracks[i].AddMinute( mTracks[i].GetFrequency() );
            continue;
        }
        else {
            if( 0 != mTracks[i].GetFrequency().toInt() )
                mTrackList.append( std::make_pair( i, mTracks[i].GetFrequency().toInt() ) );
        }
    }
}


void TrackDistributor::ranking()
{
    for( int i = 0; i < mTrackList.size(); ++i ) {
        int size = mTrackList[i].second;
        int step = 60 / size;
        int minute = rand()%(step-1);
        for( int j = 0; j < size; ++j ) {
            mTracks[mTrackList[i].first].AddMinute(QString::number(minute) + 'm' );
            minute += step;
        }
    }
}
