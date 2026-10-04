#pragma once
#include "stationmanager.h"
#include <QString>

namespace MediaBoxManager {
    enum CHANNEL_TYPE { NO_TYPE, MUSIC, VIDEO, ADVERT };
}

struct SPathData
{
    SPathData()
    {
        StationManager::Instance().update();
        homePathDir = STATIONPATH;
        channelDirMusic = STATIONMEDIATO("music/");
        channelDirVideo = STATIONMEDIATO("video/");
        advertDir = STATIONMEDIATO("ads/");
        projectFile = STATIONPATHTO("project.json");
    }
    QString homePathDir;
    QString channelDirMusic;
    QString channelDirVideo;
    QString advertDir;
    QString projectFile;
};
