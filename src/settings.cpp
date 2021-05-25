
#include "settings.h"
#include "lampdata.h"

#include <QSettings>
#include <QApplication>
#include <QDir>
#include <QDebug>


Settings::Settings(QObject *parent) :
    QObject(parent),
    mFileFormatsGroup("FileFormats"),
    mFormatsAudioGroup("AudioFormats"),
    mFormatsVideoGroup("VideoFormats")
{
    mConfigFile = qApp->applicationDirPath() + QDir::separator() + qApp->applicationName() + ".conf";
    readSettings();
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
    foreach (QString key, keys) {
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
}

void Settings::writeFileFormatVideoValue(QString format, bool value)
{
    QSettings settings( mConfigFile, QSettings::IniFormat );
    settings.beginGroup( mFileFormatsGroup);
    settings.beginGroup( mFormatsVideoGroup );
    settings.setValue(format, value);
    settings.endGroup();
    settings.endGroup();
}

void Settings::writeStringSettings(QString key, QString value)
{
    QSettings settings(mConfigFile, QSettings::IniFormat );
    settings.setValue(key, value);
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
    QSettings settings(qApp->applicationDirPath() + QDir::separator() + qApp->applicationName()+".conf", QSettings::IniFormat );
    // Форматы файлов
    settings.beginGroup("FileFormats");
    QStringList keys = settings.allKeys();
    foreach (QString key, keys) {
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
    QSettings settings(qApp->applicationDirPath() + QDir::separator() + qApp->applicationName()+".conf", QSettings::IniFormat );
    // Форматы файлов
    settings.beginGroup("FileFormats");
    settings.beginGroup("AudioFormats");

    QStringList keys = settings.allKeys();
    foreach (QString key, keys) {
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
    QSettings settings(qApp->applicationDirPath() + QDir::separator() + qApp->applicationName()+".conf", QSettings::IniFormat );
    // Форматы файлов
    settings.beginGroup("FileFormats");
    settings.beginGroup("VideoFormats");
    QStringList keys = settings.allKeys();
    foreach (QString key, keys) {
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
