
/// LampBox
#include "mediamanager.h"
#include "lampdata.h"
#include "mediamodel.h"
#include "settings.h"
#include "boxlog.h"
#include "informer.h"
#include "trialmessagebox.h"

/// Taglib
#include "tag.h"
#include "fileref.h"
#include "audioproperties.h"

/// Qt
#include <QFileInfoList>
#include <QPointer>
#include <QApplication>
#include <QProgressDialog>
#include <QSettings>
#include <QDebug>


MediaManager::MediaManager(CHANNEL_TYPE type) :
    mMediaModel(nullptr),
    mType(type)
{
    SPathData path;
    if( MUSIC == mType ) {
        mChannelDir = path.channelDirMusic;
    } else if( VIDEO == mType ) {
        mChannelDir = path.channelDirVideo;
    }

    mDir.setPath( mChannelDir );
}

MediaManager::MediaManager(QString pathDir, CHANNEL_TYPE type) :
    mMediaModel(nullptr),
    mType(type)
{
    if( pathDir.right(1) != "/" )
        pathDir += "/";
    mDirName = pathDir;
    mDir.setPath(pathDir);
    mDirMediaFiles.setPath(pathDir);
    collectMediaFiles();
}


MediaManager::~MediaManager()
{
    if(mMediaModel)
        mMediaModel->setMediaManager(0);
}

int MediaManager::columnCount()
{
    return 1;
}

void MediaManager::setMediaModel(MediaModel *model)
{
    mMediaModel = model;
}


/**
 * @brief MediaManager::collectMediaFiles
 */
void MediaManager::collectMediaFiles()
{
    mTotalLength = 0;
    if(mMediaModel)
        mMediaModel->beginCollect();

    mMediaList.clear();

    QStringList fileFormats;
    if( mType == MUSIC )
        fileFormats = Settings::allAudioFormats();
    else if( mType == VIDEO )
        fileFormats = Settings::allVideoFormats();
    else if( mType == ADVERT )
        fileFormats = Settings::allFormats();
    else
        fileFormats << "*.*";

    QFileInfoList infoList = mDirMediaFiles.entryInfoList( fileFormats, QDir::Files, QDir::Name);
    for( int i = 0; i < infoList.size(); ++i ) {
        QString fileName = infoList.at(i).absoluteFilePath();
        QString tagFileName = fileName + ".tag";
        QSettings tag( tagFileName, QSettings::IniFormat );
        if( !QFileInfo::exists(tagFileName) ) {
            createTagFile(fileName);
        }
        MediaData media;
        media.setFileName( infoList.at(i).fileName() );
        media.setFileSize( infoList.at(i).size() );
        media.setAlbum( tag.value("album").toString() );
        media.setArtist( tag.value("artist").toString() );
        media.setTitle( tag.value("title").toString() );
        media.setGenre( tag.value("genre").toString() );
        media.setYear( tag.value("year").toInt() );
        media.setLength( tag.value("length").toUInt() );

        if(StationManager::Instance().trial() ) {
            /// Ограничение файла в 5 минут
            if( media.length() > 300 ) {
                qDebug() << media.length();
                delFile(fileName);
                TrialMessageBox("Длительность композиции должна быть не более 5 минут");
                continue;
            }
        }

        mTotalLength += media.length();
        mMediaList.append( media );
    }

    if( StationManager::Instance().trial() ) {
        if( mMediaList.size() > 9 ) {
            mMediaList = mMediaList.mid(0, 10);
        }

        calculateTotalLength(mMediaList);
    }


    if(mMediaModel)
        mMediaModel->endCollect();
}

/**
 * @brief MediaManager::setChannelName
 * @param name
 */
void MediaManager::setDirName(const QString &name)
{
    setDirMediaFiles(name);
    collectMediaFiles();
}

void MediaManager::setDirMediaFiles(const QString &name)
{
    mDirName = name;
    SPathData path;
    mDirMediaFiles.setPath( mChannelDir + mDirName);
}

bool MediaManager::renameChannelDir( const QString &name )
{
    if( name == mDirName || name.isEmpty() )
        return false;

    if( mDir.exists(name) ) {
        setDirMediaFiles(name);
        return true;
    } else if( !mDir.rename(mDirName, name) )
        return false;
//        return createChannelDir( name );

    setDirMediaFiles(name);
    return true;
}

bool MediaManager::createChannelDir(const QString &name)
{
    if( name == mDirName || name.isEmpty() )
        return false;

    if(!mDir.mkdir(name)) {
        BoxLog() << "Dir" << name << "cannot create!";
        return false;
    }

    setDirMediaFiles(name);
    return true;
}

bool MediaManager::deleteChannel()
{
    if( !mDirMediaFiles.removeRecursively() ) {
        BoxLog() << "Cannot delete channel!" << mDirMediaFiles.path();
        return false;
    }

    if(mMediaModel)
        mMediaModel->beginCollect();
    mMediaList.clear();
    if(mMediaModel)
        mMediaModel->endCollect();

    return true;
}

bool MediaManager::addFile(const QString &fileName)
{
    if( mDirName.isEmpty() )
        return false;

    if( !mDir.exists(mDirName) )
        if(!mDirMediaFiles.mkpath( mDirMediaFiles.path() ) )
            return false;

    // Проверка форматов файлов
    Settings settings;
    QStringList fileFormats;
    if( mType == MUSIC )
        fileFormats = settings.availablelAudioFileFormats();
    else if( mType == VIDEO )
        fileFormats = settings.availablelVideoFileFormats();
    else if( mType == ADVERT )
        fileFormats = settings.availablelAllFileFormats();

    if( !fileFormats.isEmpty() ) {
        QString ext  = "*." + fileName.section('.', -1 );
        if( !fileFormats.contains(ext, Qt::CaseInsensitive ) )
            return false;
    }

    qApp->processEvents();
    QPointer<QFile> file = new QFile(fileName);
    QString name = fileName.section('/', -1);

    name.remove('}').remove('{');
    QString mediaFileName =  mDirMediaFiles.path() + '/' + name;
    if( !file->copy( mediaFileName ) ) {
        // TODO: такой файл уже существует, переписать?
        BoxLog() << file->errorString();
        BoxLog() << "Copy error:" << fileName << "!";
        return false;
    }

    createTagFile(mediaFileName);
    qApp->processEvents();
    return true;
}


void MediaManager::createTagFile( const QString &fileName )
{
    QString tagFileName = fileName + ".tag";
    QSettings tag( tagFileName, QSettings::IniFormat);
    TagLib::FileRef mediaTag(fileName.toStdWString().c_str() );
    if( mediaTag.isNull() )
        return;
    if( !mediaTag.tag()->isEmpty() ) {
        tag.setValue( "album",      QString::fromStdWString( mediaTag.tag()->album().toWString()   ) );
        tag.setValue( "artist",     QString::fromStdWString( mediaTag.tag()->artist().toWString()  ) );
        tag.setValue( "title",      QString::fromStdWString( mediaTag.tag()->title().toWString()   ) );
        tag.setValue( "genre",      QString::fromStdWString( mediaTag.tag()->genre().toWString()   ) );
        tag.setValue( "year",       mediaTag.tag()->year() );
    }
    if( !mediaTag.audioProperties() )
        return;
    tag.setValue( "length",     mediaTag.audioProperties()->length() );
}

bool MediaManager::delFile(int num)
{
    if( num < 0 || num >= mMediaList.size() )
        return false;

    if(mMediaModel)
        mMediaModel->beginCollect();

    if( delFile(mMediaList.at(num).fileName()) )
        mMediaList.removeAt(num);

    if(mMediaModel)
        mMediaModel->endCollect();

    return true;
}

bool MediaManager::delFile(const QString &fileName)
{
    if( !mDirMediaFiles.remove( fileName ) ) {
        BoxLog() << "cannot remove file: " << fileName << "!";
        return false;
    }
    mDirMediaFiles.remove(fileName+".tag");
    return true;
}

/**
 * @brief MediaManager::getMediaData
 * @param id
 * @return
 */
const MediaData& MediaManager::mediaData( int id )
{
    return mMediaList[id];
}

/**
 * @brief MediaManager::mediaCont
 * @return
 */
int MediaManager::mediaCount()
{
    return mMediaList.size();
}

/**
 * @brief MediaManager::getDirMediaFiles
 * @return
 */
QDir MediaManager::getDirMediaFiles()
{
    return mDirMediaFiles;
}

/**
 * @brief count         - Количество файлов
 * @return
 */
int MediaManager::count()
{
    return mMediaList.size();
}

QString MediaManager::calculateLength( uint length )
{
    int h   = length / 3600;
    int sh  = length % 3600;
    int m   = sh / 60;
    int s   = sh % 60;

    QString minuts;
    QString res;
    if( h ) {
        res += QString::number(h) + ":";
        minuts      = QString::number( m ).rightJustified( 2, '0' );
    } else {
        minuts      = QString::number(m);
    }
    QString seconds     = QString::number( s ).rightJustified( 2, '0' );
    res += minuts + ":" + seconds;

return res;
}

QString MediaManager::totalLength()
{
    return calculateLength(mTotalLength);
}


uint MediaManager::calculateTotalLength( const QList<MediaData> &list )
{
    mTotalLength = 0;
    int size = list.size();
    for(int i = 0; i < size; ++i ) {
        mTotalLength += list[i].length();
    }
    return mTotalLength;
}
