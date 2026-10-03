#include <QSettings>
#include <QFileInfo>
#include <QStandardPaths>
#include "stationmanager.h"
#include <utility>

bool StationManager::update()
{
#ifdef Q_OS_WIN
    QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\LampBox\\Station", QSettings::NativeFormat);
    const QString configuredPath = settings.value("Path").toString();
    const TypeStation configuredType = settings.value("Type").toInt()
            == std::to_underlying(STATION_NETWORK) ? STATION_NETWORK : STATION_LOCAL;
    const bool configuredTrial = settings.value("Trial", false).toBool();
    if (settings.status() == QSettings::NoError
            && loadConfiguration(configuredPath, configuredType, configuredTrial))
        return true;

    if (loadConfiguration(QStringLiteral("C:/myplayer"), STATION_LOCAL, false))
        return true;
#elif defined(Q_OS_LINUX)
    if (loadConfiguration(QStringLiteral("/home/mediabox"), STATION_LOCAL, false))
        return true;
#endif
    return initializeStandaloneConfiguration();
}

bool StationManager::loadConfiguration(const QString &stationPath, TypeStation stationType, bool isTrial)
{
    if (stationPath.trimmed().isEmpty() || !QDir::isAbsolutePath(stationPath))
        return false;

    const QDir stationDirectory(QDir::cleanPath(stationPath));
    const QString configuration = stationDirectory.absoluteFilePath("mediabox.conf");
    const QFileInfo configurationInfo(configuration);
    if (!configurationInfo.isFile() || !configurationInfo.isReadable())
        return false;

    QSettings settings(configuration, QSettings::IniFormat);
    const int configuredId = settings.value("mediastation/mediabox_id", LOCAL_ID).toInt();
    const QString configuredName = settings.value("mediastation/mediabox_name", "NO SET").toString();
    QString mediaPath = settings.value("mediastation/media").toString();
    QString cronPath = settings.value("mediastation/crondir").toString();
    const bool configuredAlternative = settings.value("mediastation/alternative", false).toBool();
    if (settings.status() != QSettings::NoError)
        return false;

    if (mediaPath.trimmed().isEmpty())
        mediaPath = "media";
    if (cronPath.trimmed().isEmpty())
        cronPath = "cron";

    pathToStation = stationDirectory;
    pathToMedia.setPath(QDir::cleanPath(stationDirectory.absoluteFilePath(mediaPath)));
    mCronDir = QDir::cleanPath(stationDirectory.absoluteFilePath(cronPath));
    mConfigFile = configuration;
    stationId = configuredId;
    nameStation = configuredName;
    typeStation = stationType;
    mTrial = isTrial;
    alternativeExecScript = configuredAlternative;
    lastErrorStr.clear();
    return true;
}

bool StationManager::initializeStandaloneConfiguration()
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataPath.isEmpty())
        dataPath = QDir::home().absoluteFilePath(".lampbox");

    pathToStation.setPath(QDir::cleanPath(dataPath));
    pathToMedia.setPath(pathToStation.absoluteFilePath("media"));
    mCronDir = pathToStation.absoluteFilePath("cron");
    mConfigFile = pathToStation.absoluteFilePath("mediabox.conf");
    stationId = LOCAL_ID;
    nameStation = "NO SET";
    typeStation = STATION_LOCAL;
    mTrial = false;
    alternativeExecScript = false;
    lastErrorStr.clear();

    const QStringList directories = {"timetable", "media/music", "media/video", "media/ads", "nncronlt", "cron"};
    for (const QString &directory : directories) {
        if (!pathToStation.mkpath(directory)) {
            lastErrorStr = tr("Не удалось создать каталог данных: %1")
                    .arg(pathToStation.absoluteFilePath(directory));
            return false;
        }
    }
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
