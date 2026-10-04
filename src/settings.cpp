#include <utility>
#include <stdexcept>

#include "settings.h"
#include "mediaboxmanagerdata.h"
#include "storagepaths.h"

#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>
#include <QJsonDocument>

namespace {
bool syncSettings(QSettings &settings)
{
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qWarning().noquote() << QStringLiteral("Не удалось сохранить настройки в %1 (код %2).")
                .arg(settings.fileName()).arg(static_cast<int>(settings.status()));
    }
    return settings.status() == QSettings::NoError;
}
}


Settings::Settings(QObject *parent) :
    Settings(configurationFilePath(), parent)
{
}

Settings::Settings(const QString &configurationFile, QObject *parent) :
    QObject(parent),
    mConfigFile(configurationFile),
    mFileFormatsGroup("FileFormats"),
    mFormatsAudioGroup("AudioFormats"),
    mFormatsVideoGroup("VideoFormats")
{
    readSettings();
}

QString Settings::configurationFilePath()
{
    const QString previewFile = QCoreApplication::instance()->property("restylePreviewSettings").toString();
    if (QStandardPaths::isTestModeEnabled() && !previewFile.isEmpty()
            && QDir::isAbsolutePath(previewFile))
        return previewFile;
    const QString directory = MediaBox::StoragePaths::configurationDirectory(
            MediaBox::StoragePaths::Application::Manager);
    const QString file = QDir(directory).absoluteFilePath("MediaBoxManager.conf");
    if (QFileInfo::exists(file) && !QFileInfo(file).isFile())
        throw std::runtime_error(QStringLiteral("Путь настроек не является файлом: %1").arg(file).toUtf8().constData());
    if (!QDir().mkpath(directory))
        throw std::runtime_error(QStringLiteral("Не удалось создать каталог настроек: %1").arg(directory).toUtf8().constData());
    return file;
}


void Settings::initSettings()
{
    QFile file(mConfigFile);
//    file.remove();
    if( !file.exists() ) {
        writeFileFormatAudioValue( "acc",  false  );
        writeFileFormatAudioValue( "flac", false  );
        writeFileFormatAudioValue( "mp3",  true   ); ///
        writeFileFormatAudioValue( "ogg",  false  );
        writeFileFormatAudioValue( "wma",  false  );
        ///
        writeFileFormatVideoValue( "mp4",  true   ); ///
        writeFileFormatVideoValue( "avi",  false  );
        writeFileFormatVideoValue( "mkv",  false  );
        writeFileFormatVideoValue( "wmv",  false  );
    }
}

void Settings::readSettings()
{
    initSettings();

    QSettings settings(mConfigFile, QSettings::IniFormat );

    // Форматы файлов
    mFileFormats.clear();
    mFileAudioFormats.clear();
    mFileVideoFormats.clear();
    settings.beginGroup(mFileFormatsGroup);
    QStringList keys = settings.allKeys();
    for (const QString &key : std::as_const(keys)) {
        QString group = key.section('/', -2, -2);
        QString fileFormat = key.section( '/', -1);
        mFileFormats.insert( fileFormat, settings.value(key, false).toBool() );
        if( group == mFormatsAudioGroup )
            mFileAudioFormats.insert( fileFormat, settings.value(key, false).toBool() );
        else if( group == mFormatsVideoGroup )
            mFileVideoFormats.insert( fileFormat, settings.value(key, false).toBool() );
    }
    settings.endGroup();
}

void Settings::writeFileFormatAudioValue(QString format, bool value)
{
    QSettings settings( mConfigFile, QSettings::IniFormat );
    settings.beginGroup( mFileFormatsGroup );
    settings.beginGroup( mFormatsAudioGroup );
    settings.setValue( format, value );
    settings.endGroup();
    settings.endGroup();
    syncSettings(settings);
}

void Settings::writeFileFormatVideoValue(QString format, bool value)
{
    QSettings settings( mConfigFile, QSettings::IniFormat );
    settings.beginGroup( mFileFormatsGroup);
    settings.beginGroup( mFormatsVideoGroup );
    settings.setValue(format, value);
    settings.endGroup();
    settings.endGroup();
    syncSettings(settings);
}

void Settings::writeStringSettings(QString key, QString value)
{
    QSettings settings(mConfigFile, QSettings::IniFormat );
    settings.setValue(key, value);
    syncSettings(settings);
}

QString Settings::appearanceId() const
{
    QSettings settings(mConfigFile, QSettings::IniFormat);
    const QString id = settings.value("Appearance/style", "tide-relief").toString();
    return id == "tide" ? id : QStringLiteral("tide-relief");
}

QString Settings::themeId() const
{
    QSettings settings(mConfigFile, QSettings::IniFormat);
    const QString id = settings.value("Appearance/theme", "denim").toString();
    static const QStringList supported = {"denim", "slate", "pine", "berry",
                                           "graphite", "pearl", "dark"};
    return supported.contains(id) ? id : QStringLiteral("denim");
}

void Settings::setAppearanceId(const QString &id)
{
    if (id == "tide" || id == "tide-relief")
        writeStringSettings("Appearance/style", id);
}

void Settings::setThemeId(const QString &id)
{
    static const QStringList supported = {"denim", "slate", "pine", "berry",
                                           "graphite", "pearl", "dark"};
    if (supported.contains(id))
        writeStringSettings("Appearance/theme", id);
}

QByteArray Settings::mainWindowGeometry() const
{
    QSettings settings(mConfigFile, QSettings::IniFormat);
    return settings.value("MainWindow/Geometry").toByteArray();
}

bool Settings::setMainWindowGeometry(const QByteArray &geometry)
{
    QSettings settings(mConfigFile, QSettings::IniFormat);
    if (geometry.isEmpty())
        settings.remove("MainWindow/Geometry");
    else
        settings.setValue("MainWindow/Geometry", geometry);
    return syncSettings(settings);
}

PlayerConnectionSettings Settings::playerConnection() const
{
    QSettings settings(mConfigFile, QSettings::IniFormat);
    PlayerConnectionSettings connection;
    connection.host = settings.value("Player/Host", connection.host).toString();
    bool portValid = false;
    const int port = settings.value("Player/Port", connection.port).toInt(&portValid);
    if (portValid && port > 0 && port <= 65535)
        connection.port = static_cast<quint16>(port);
    connection.token = settings.value("Player/Token").toString();
    return connection;
}

bool Settings::setPlayerConnection(const PlayerConnectionSettings &connection)
{
    QSettings settings(mConfigFile, QSettings::IniFormat);
    settings.setValue("Player/Host", connection.host.trimmed());
    settings.setValue("Player/Port", connection.port);
    settings.setValue("Player/Token", connection.token.trimmed());
    return syncSettings(settings);
}

PlayerConnectionSettings Settings::videoPlayerConnection() const
{
    QSettings settings(mConfigFile, QSettings::IniFormat);
    PlayerConnectionSettings connection;
    connection.port = 17656;
    connection.host = settings.value("VideoPlayer/Host", connection.host).toString();
    bool valid = false;
    const int port = settings.value("VideoPlayer/Port", connection.port).toInt(&valid);
    if (valid && port > 0 && port <= 65535)
        connection.port = static_cast<quint16>(port);
    connection.token = settings.value("VideoPlayer/Token").toString();
    return connection;
}

bool Settings::setVideoPlayerConnection(const PlayerConnectionSettings &connection)
{
    QSettings settings(mConfigFile, QSettings::IniFormat);
    settings.setValue("VideoPlayer/Host", connection.host.trimmed());
    settings.setValue("VideoPlayer/Port", connection.port);
    settings.setValue("VideoPlayer/Token", connection.token.trimmed());
    return syncSettings(settings);
}

QJsonArray Settings::videoWindowProfiles() const
{
    QSettings settings(mConfigFile, QSettings::IniFormat);
    return QJsonDocument::fromJson(settings.value("VideoPlayer/Profiles").toByteArray()).array();
}

bool Settings::setVideoWindowProfiles(const QJsonArray &profiles)
{
    QSettings settings(mConfigFile, QSettings::IniFormat);
    settings.setValue("VideoPlayer/Profiles", QJsonDocument(profiles).toJson(QJsonDocument::Compact));
    return syncSettings(settings);
}

const QMap<QString, bool> &Settings::fileFormats()
{
    return mFileFormats;
}

const QMap<QString, bool> &Settings::fileFormatsAudio()
{
    return mFileAudioFormats;
}

const QMap<QString, bool> &Settings::fileFormatsVideo()
{
    return mFileVideoFormats;
}

const QStringList Settings::availableFileFormats(QMap<QString, bool> fileFormats)
{
    QStringList list;
    auto iterator = fileFormats.begin();

    while( iterator != fileFormats.end() ) {
        if( iterator.value() )
            list.append("*." + iterator.key());
        ++iterator;
    }

    return list;
}

const QStringList Settings::availablelAllFileFormats()
{
    QStringList list = availableFileFormats(mFileFormats);
    if(list.isEmpty()) {
        list.append(DEFAULT_AUDIO_FORMAT);
        list.append(DEFAULT_VIDEO_FORMAT);
    }
    return list;
}

const QStringList Settings::availablelAudioFileFormats()
{
    QStringList list = availableFileFormats(mFileAudioFormats);
    if(list.isEmpty()) {
        list.append(DEFAULT_AUDIO_FORMAT);
    }
    return list;
}

const QStringList Settings::availablelVideoFileFormats()
{
    QStringList list = availableFileFormats(mFileVideoFormats);
    if(list.isEmpty()) {
        list.append(DEFAULT_VIDEO_FORMAT);
    }
    return list;
}

/*! Static Members
*/
QStringList Settings::allFormats()
{
    QStringList list;
    QSettings settings(configurationFilePath(), QSettings::IniFormat);
    // Форматы файлов
    settings.beginGroup("FileFormats");
    QStringList keys = settings.allKeys();
    for (const QString &key : std::as_const(keys)) {
//        QString group = key.section('/', -2);
        QString fileFormat = key.section('/', -1);
        list << "*." + fileFormat;
    }
    settings.endGroup();
    if(list.isEmpty()) {
        list << DEFAULT_AUDIO_FORMAT;
        list << DEFAULT_VIDEO_FORMAT;
    }
    return list;
}

QStringList Settings::allAudioFormats()
{
    QStringList list;
    QSettings settings(configurationFilePath(), QSettings::IniFormat);
    // Форматы файлов
    settings.beginGroup("FileFormats");
    settings.beginGroup("AudioFormats");

    QStringList keys = settings.allKeys();
    for (const QString &key : std::as_const(keys)) {
//        QString group = key.section('/', -2);
        QString fileFormat = key.section('/', -1);
        list << "*." + fileFormat;
    }
    settings.endGroup();
    settings.endGroup();
    if(list.isEmpty()) {
        list << DEFAULT_AUDIO_FORMAT;
    }
    return list;
}

QStringList Settings::allVideoFormats()
{
    QStringList list;
    QSettings settings(configurationFilePath(), QSettings::IniFormat);
    // Форматы файлов
    settings.beginGroup("FileFormats");
    settings.beginGroup("VideoFormats");
    QStringList keys = settings.allKeys();
    for (const QString &key : std::as_const(keys)) {
        QString fileFormat = key.section('/', -1);
        list << "*." + fileFormat;
    }
    settings.endGroup();
    settings.endGroup();
    if(list.isEmpty()) {
        list << DEFAULT_VIDEO_FORMAT;
    }
    return list;
}
