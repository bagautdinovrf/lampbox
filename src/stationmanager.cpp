#include <QSettings>
#include <QDebug>
#include "stationmanager.h"

bool StationManager::update()
{
#ifdef WIN32
    QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Lampmedia\\Station", QSettings::NativeFormat);
    if( settings.status() == QSettings::NoError) {
        pathToStation.setPath( settings.value("Path").toString() );
//        qDebug() << pathToStation.absolutePath();
        if( settings.value("Type").toInt() == (int)STATION_LOCAL){ //local
            typeStation = STATION_LOCAL;
        } else if( settings.value("Type").toInt() == (int)STATION_NETWORK){ //network
            typeStation = STATION_NETWORK;
        }

        mTrial = settings.value("Trial", false).toBool();

        mConfigFile = pathToStation.absoluteFilePath("mediabox.conf");
        QSettings settingfile(mConfigFile, QSettings::IniFormat);
        if( settingfile.status() == QSettings::NoError){
            stationId = settingfile.value("mediastation/mediabox_id", LOCAL_ID).toInt();
            pathToMedia.setPath(settingfile.value("mediastation/media").toString());
            nameStation = settingfile.value("mediastation/mediabox_name").toString();
            alternativeExecScript = settingfile.value("mediastation/alternative").toBool();
            mCronDir=settingfile.value("mediastation/crondir","c:/myplayer/cron").toString();
            if(pathToStation.absolutePath().isEmpty()){
                lastErrorStr = tr("<b>LampPlayer</b> был удален или поврежден.<br>"
                                  "Для востановления необходимо переустановить <b>LampPlayer</b>.<br>");
                return false;
            }
        } else {
            lastErrorStr = tr("<b>LampPlayer</b> был удален или поврежден.<br>"
                              "Для востановления необходимо переустановить <b>LampPlayer</b>.<br>");
            return false;
        }
    } else {
        lastErrorStr = tr("<b>LampPlayer</b> не установлен.");
        return false;
    }
#elif __linux__
    pathToStation = "/home/mediabox/";
    pathToMedia = "/home/mediabox/music/";
#endif
    return true;
}

QString StationManager::get()
{
return pathToStation.absolutePath();
}

QString StationManager::get(QString subdir){
//    qDebug() << pathToStation.absoluteFilePath(subdir);
return pathToStation.absoluteFilePath(subdir);
}

TypeStation StationManager::type(){
    return typeStation;
}

QString StationManager::getCronDir(){
    return mCronDir;
}

int StationManager::id(){
    return stationId;
}
QString StationManager::typeText(){
    return textType[type()];
}

QString StationManager::lastError(){
return lastErrorStr;
}

QString StationManager::media(QString subdir){
return pathToMedia.absoluteFilePath(subdir);
}

bool StationManager::isAlter(){
        return alternativeExecScript;
}

QString StationManager::configFile()
{
    return mConfigFile;
}

bool StationManager::trial()
{
    return mTrial;
}
