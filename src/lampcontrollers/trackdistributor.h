#ifndef TRACKDISTRIBUTOR_H
#define TRACKDISTRIBUTOR_H

#include "trackfullinfo.h"

#include <QObject>
#include <QList>

#include <cstddef>
#include <utility>

class TrackDistributor : public QObject
{
    Q_OBJECT
    using FrequncyTable = std::vector<std::vector<int>>;
public:
    explicit TrackDistributor( TracksFullInfo &tracks, QObject *parent = nullptr);

    TracksFullInfo& distribute();

private:
    void counting();

    void ranking();

private:
    TracksFullInfo          &mTracks;
    QList<std::pair<std::size_t, int>> mTrackList;
};

#endif // TRACKDISTRIBUTOR_H
