#include <utility>

#include "settings.h"
#include "lampdata.h"

#include <QSettings>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace {
bool isProgramFilesDirectory(const QDir &directory)
{
#ifdef Q_OS_WIN
    const QString applicationPath = QDir::cleanPath(directory.absolutePath());
    const char *variables[] = {"ProgramW6432", "ProgramFiles", "ProgramFiles(x86)"};
    for (const char *variable : variables) {
        const QString configuredRoot = QDir::fromNativeSeparators(qEnvironmentVariable(variable));
        if (configuredRoot.isEmpty() || !QDir::isAbsolutePath(configuredRoot))
            continue;
        const QString root = QDir::cleanPath(QDir(configuredRoot).absolutePath());
        const QString prefix = root.endsWith('/') ? root : root + '/';
        if (applicationPath.compare(root, Qt::CaseInsensitive) == 0
                || applicationPath.startsWith(prefix, Qt::CaseInsensitive))
            return true;
    }
#else
    Q_UNUSED(directory);
#endif
    return false;
}

QString programDataDirectory()
{
#ifdef Q_OS_WIN
    const QString configuredDirectory = QDir::fromNativeSeparators(qEnvironmentVariable("ProgramData"));
    if (!configuredDirectory.isEmpty() && QDir::isAbsolutePath(configuredDirectory))
        return QDir::cleanPath(configuredDirectory);

    wchar_t commonDataPath[MAX_PATH] = {};
    const HRESULT result = SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr,
                                          SHGFP_TYPE_CURRENT, commonDataPath);
    if (SUCCEEDED(result)) {
        const QString systemDirectory = QDir::fromNativeSeparators(QString::fromWCharArray(commonDataPath));
        if (!systemDirectory.isEmpty() && QDir::isAbsolutePath(systemDirectory))
            return QDir::cleanPath(systemDirectory);
    }
    qWarning().noquote() << QStringLiteral(
            "Не удалось определить общий каталог ProgramData (код %1); "
            "будет использован пользовательский каталог настроек.")
            .arg(QString::number(static_cast<quint32>(result), 16));
#endif
    return {};
}

void syncSettings(QSettings &settings)
{
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qWarning().noquote() << QStringLiteral("Не удалось сохранить настройки в %1 (код %2).")
                .arg(settings.fileName()).arg(static_cast<int>(settings.status()));
    }
}
}


Settings::Settings(QObject *parent) :
    Settings(configurationFilePath(qApp->applicationDirPath()), parent)
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

QString Settings::configurationFilePath(const QString &applicationDirectory)
{
    // The dedicated preview harness supplies an owned temporary path before
    // any singleton is constructed. Normal application startup never sets it.
    const QString previewFile = QCoreApplication::instance()->property("restylePreviewSettings").toString();
    if (QStandardPaths::isTestModeEnabled() && !previewFile.isEmpty()
            && QDir::isAbsolutePath(previewFile))
        return previewFile;
    return configurationFilePath(applicationDirectory,
                                 QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
}

QString Settings::configurationFilePath(const QString &applicationDirectory,
                                       const QString &previousUserConfigurationDirectory)
{
    const QDir directory(applicationDirectory);
    const QString applicationName = QCoreApplication::applicationName();
    QString preferredFile = directory.absoluteFilePath(applicationName + ".conf");
    if (applicationName == QStringLiteral("MediaBoxManager")
            && !QFileInfo(preferredFile).isFile()) {
        const QString legacyFile = directory.absoluteFilePath("lampbox.conf");
        if (QFileInfo(legacyFile).isFile())
            preferredFile = legacyFile;
    }
    if (applicationName == QStringLiteral("MediaBoxManager") && isProgramFilesDirectory(directory)) {
        const QString commonDirectory = programDataDirectory();
        QString configurationDirectory;
        if (!commonDirectory.isEmpty()) {
            configurationDirectory = QDir(commonDirectory).absoluteFilePath("MediaBox");
        } else {
            configurationDirectory = previousUserConfigurationDirectory;
            if (configurationDirectory.isEmpty() || !QDir::isAbsolutePath(configurationDirectory)
                    || isProgramFilesDirectory(QDir(configurationDirectory)))
                configurationDirectory = QDir::home().absoluteFilePath(".mediaboxmanager");
        }
        const QDir configurationDir(configurationDirectory);
        const QString configurationFile = configurationDir.absoluteFilePath("MediaBoxManager.conf");
        if (QFileInfo::exists(configurationFile)) {
            if (!QFileInfo(configurationFile).isFile()) {
                qWarning().noquote() << QStringLiteral("Путь настроек не является файлом: %1")
                        .arg(configurationFile);
            }
            return configurationFile;
        }
        if (!configurationDir.mkpath(".")) {
            qWarning().noquote() << QStringLiteral("Не удалось создать каталог настроек: %1")
                    .arg(configurationDirectory);
            return configurationFile;
        }
        QStringList previousFiles;
        if (!previousUserConfigurationDirectory.isEmpty())
            previousFiles << QDir(previousUserConfigurationDirectory).absoluteFilePath("MediaBoxManager.conf");
        previousFiles << directory.absoluteFilePath("MediaBoxManager.conf")
                      << directory.absoluteFilePath("lampbox.conf");
        for (const QString &previousFile : std::as_const(previousFiles)) {
            if (!QFileInfo(previousFile).isFile())
                continue;
            QFile source(previousFile);
            if (!source.copy(configurationFile)) {
                qWarning().noquote() << QStringLiteral(
                        "Не удалось скопировать настройки из %1 в %2: %3. "
                        "Исходный файл сохранён.")
                        .arg(previousFile, configurationFile, source.errorString());
            }
            break;
        }
        return configurationFile;
    }
    return preferredFile;
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
    QSettings settings(configurationFilePath(qApp->applicationDirPath()), QSettings::IniFormat);
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
    QSettings settings(configurationFilePath(qApp->applicationDirPath()), QSettings::IniFormat);
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
    QSettings settings(configurationFilePath(qApp->applicationDirPath()), QSettings::IniFormat);
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
