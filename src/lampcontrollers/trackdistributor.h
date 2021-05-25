#ifndef TRACKDISTRIBUTOR_H
#define TRACKDISTRIBUTOR_H

#include "trackfullinfo.h"

#include <QObject>
#include <QList>

#include <map>

class TrackDistributor : public QObject
{
    Q_OBJECT
    typedef std::vector< std::vector<int> > FrequncyTable;
public:
    explicit TrackDistributor( TracksFullInfo &tracks, QObject *parent = 0);

    TracksFullInfo& distribute();

private:
    void clear();

    void counting();

    void ranking();

private:
    TracksFullInfo          &mTracks;
    QList<std::pair<int, int> > mTrackList;
};

#endif // TRACKDISTRIBUTOR_H
