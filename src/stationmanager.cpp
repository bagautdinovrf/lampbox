#include <QSettings>
#include <QCoreApplication>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include "stationmanager.h"
#include "storagepaths.h"
#include <utility>

namespace {
const QStringList stationEntries = {QStringLiteral("timetable"), QStringLiteral("media"),
        QStringLiteral("cron"), QStringLiteral("nncronlt"), QStringLiteral("mediabox.conf")};
const QString initializedMarker = QStringLiteral(".station-storage-initialized");
const QString migrationMarker = QStringLiteral(".station-storage-migration");

bool containsFiles(const QFileInfo &entry)
{
    // A link is data too, but migration will report it instead of following it.
    if (entry.isSymLink())
        return true;
    if (!entry.exists())
        return false;
    if (!entry.isDir())
        return true;
    if (!entry.isReadable())
        return true;
    const QFileInfoList children = QDir(entry.absoluteFilePath()).entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &child : children) {
        if (containsFiles(child))
            return true;
    }
    return false;
}

bool containsStationData(const QDir &directory)
{
    for (const QString &entry : stationEntries) {
        if (containsFiles(QFileInfo(directory.absoluteFilePath(entry))))
            return true;
    }
    return false;
}

bool writeMarker(const QString &path, const QByteArray &contents, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()
            || !file.commit()) {
        *error = QObject::tr("Не удалось сохранить состояние переноса данных %1: %2")
                .arg(path, file.errorString());
        return false;
    }
    return true;
}

bool copyStationEntry(const QString &source, const QString &destination, QString *error)
{
    const QFileInfo sourceInfo(source);
    const QFileInfo destinationInfo(destination);
    if (sourceInfo.isSymLink() || destinationInfo.isSymLink()) {
        *error = QObject::tr("Не удалось перенести символическую ссылку: %1").arg(source);
        return false;
    }
    if (!sourceInfo.exists())
        return true;
    if (sourceInfo.isDir()) {
        if (!sourceInfo.isReadable() || (destinationInfo.exists() && !destinationInfo.isDir())
                || !QDir().mkpath(destination)) {
            *error = QObject::tr("Не удалось перенести каталог данных: %1").arg(source);
            return false;
        }
        const QFileInfoList children = QDir(source).entryInfoList(
                QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &child : children) {
            if (!copyStationEntry(child.absoluteFilePath(),
                                  QDir(destination).absoluteFilePath(child.fileName()), error))
                return false;
        }
        return true;
    }
    if (!sourceInfo.isFile() || (destinationInfo.exists() && !destinationInfo.isFile())) {
        *error = QObject::tr("Не удалось перенести файл данных: %1").arg(source);
        return false;
    }
    return MediaBox::StoragePaths::migrateFile(destination, {source}, error);
}

bool adjustMigratedConfiguration(const QDir &source, const QDir &destination, QString *error)
{
    const QString sourcePath = source.absoluteFilePath("mediabox.conf");
    if (!QFileInfo::exists(sourcePath))
        return true;
    QSettings original(sourcePath, QSettings::IniFormat);
    QSettings migrated(destination.absoluteFilePath("mediabox.conf"), QSettings::IniFormat);
    const QStringList keys = {QStringLiteral("mediastation/media"),
                              QStringLiteral("mediastation/crondir")};
    for (const QString &key : keys) {
        const QString configuredPath = original.value(key).toString();
        if (configuredPath.trimmed().isEmpty())
            continue;
        const QString absolutePath = QDir::cleanPath(source.absoluteFilePath(configuredPath));
        const QString relativePath = source.relativeFilePath(absolutePath);
        bool copied = false;
        for (const QString &entry : stationEntries) {
            if (entry != QStringLiteral("mediabox.conf")
                    && (relativePath == entry || relativePath.startsWith(entry + '/'))) {
                copied = true;
                break;
            }
        }
        // Explicit external content stays where it was. Paths into copied data
        // follow the new station, including absolute paths in old configurations.
        migrated.setValue(key, copied ? relativePath : absolutePath);
    }
    migrated.sync();
    if (original.status() != QSettings::NoError || migrated.status() != QSettings::NoError) {
        *error = QObject::tr("Не удалось перенести конфигурацию станции: %1").arg(sourcePath);
        return false;
    }
    return true;
}
}

bool StationManager::update()
{
    // The dedicated GUI verification executable supplies a temporary station.
    // Fail closed so a broken fixture can never fall back to a real station.
    const QString previewStation = QCoreApplication::instance()
            ? QCoreApplication::instance()->property("restylePreviewStation").toString()
            : QString();
    if (!previewStation.isEmpty()) {
        if (!QStandardPaths::isTestModeEnabled()
                || !QDir::isAbsolutePath(previewStation)
                || !loadConfiguration(previewStation, STATION_LOCAL, false))
            qFatal("Invalid isolated preview station");
        return true;
    }
    if (!QStandardPaths::isTestModeEnabled()) {
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
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        if (loadConfiguration(QStringLiteral("/home/mediabox"), STATION_LOCAL, false))
            return true;
#endif
    }
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
    const QString oldDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QStringList legacyPaths;
    if (!oldDataPath.isEmpty()) {
        legacyPaths.append(oldDataPath);
        legacyPaths.append(QFileInfo(oldDataPath).dir().absoluteFilePath("lampbox"));
    }
    if (!QStandardPaths::isTestModeEnabled()) {
        legacyPaths.append(QDir::home().absoluteFilePath(".mediaboxmanager"));
        legacyPaths.append(QDir::home().absoluteFilePath(".lampbox"));
    }
    return initializeStandaloneConfiguration(MediaBox::StoragePaths::commonDataDirectory(), legacyPaths);
}

bool StationManager::initializeStandaloneConfiguration(const QString &dataPath,
                                                       const QStringList &legacyPaths)
{
    lastErrorStr.clear();
    if (dataPath.isEmpty() || !QDir::isAbsolutePath(dataPath) || !QDir().mkpath(dataPath)) {
        lastErrorStr = tr("Не удалось создать каталог данных: %1").arg(dataPath);
        return false;
    }
    const QDir destination(QDir::cleanPath(dataPath));
    const QString initializedPath = destination.absoluteFilePath(initializedMarker);
    const QString pendingPath = destination.absoluteFilePath(migrationMarker);
    if (!QFileInfo::exists(initializedPath)) {
        QString sourcePath;
        if (QFileInfo::exists(pendingPath)) {
            QFile pending(pendingPath);
            if (!pending.open(QIODevice::ReadOnly)) {
                lastErrorStr = tr("Не удалось прочитать состояние переноса данных: %1").arg(pendingPath);
                return false;
            }
            sourcePath = QString::fromUtf8(pending.readAll());
            if (!QDir::isAbsolutePath(sourcePath) || !QFileInfo(sourcePath).isDir()) {
                lastErrorStr = tr("Недоступен исходный каталог переноса данных: %1").arg(sourcePath);
                return false;
            }
        } else if (!containsStationData(destination)) {
            for (const QString &candidate : legacyPaths) {
                if (!QDir::isAbsolutePath(candidate)
                        || QDir::cleanPath(candidate) == destination.absolutePath())
                    continue;
                if (containsStationData(QDir(candidate))) {
                    sourcePath = QDir::cleanPath(candidate);
                    break;
                }
            }
            if (!sourcePath.isEmpty() && !writeMarker(pendingPath, sourcePath.toUtf8(), &lastErrorStr))
                return false;
        }
        if (!sourcePath.isEmpty()) {
            const QDir source(sourcePath);
            for (const QString &entry : stationEntries) {
                if (!copyStationEntry(source.absoluteFilePath(entry),
                                      destination.absoluteFilePath(entry), &lastErrorStr))
                    return false;
            }
            if (!adjustMigratedConfiguration(source, destination, &lastErrorStr))
                return false;
        }
    }

    pathToStation = destination;
    pathToMedia.setPath(pathToStation.absoluteFilePath("media"));
    mCronDir = pathToStation.absoluteFilePath("cron");
    mConfigFile = pathToStation.absoluteFilePath("mediabox.conf");
    stationId = LOCAL_ID;
    nameStation = "NO SET";
    typeStation = STATION_LOCAL;
    mTrial = false;
    alternativeExecScript = false;

    if (QFileInfo::exists(mConfigFile) && !loadConfiguration(dataPath, STATION_LOCAL, false)) {
        lastErrorStr = tr("Не удалось загрузить конфигурацию станции: %1").arg(mConfigFile);
        return false;
    }

    const QStringList directories = {"timetable", "media/music", "media/video", "media/ads", "nncronlt", "cron"};
    for (const QString &directory : directories) {
        if (!pathToStation.mkpath(directory)) {
            lastErrorStr = tr("Не удалось создать каталог данных: %1")
                    .arg(pathToStation.absoluteFilePath(directory));
            return false;
        }
    }
    if (!QFileInfo::exists(initializedPath)
            && !writeMarker(initializedPath, QByteArrayLiteral("1\n"), &lastErrorStr))
        return false;
    if (QFileInfo::exists(pendingPath) && !QFile::remove(pendingPath)) {
        lastErrorStr = tr("Не удалось завершить перенос данных: %1").arg(pendingPath);
        return false;
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
