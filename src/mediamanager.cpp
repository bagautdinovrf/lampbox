
#include "mediamanager.h"
#include "mediaboxmanagerdata.h"
#include "mediamodel.h"
#include "mediaimportservice.h"
#include "settings.h"
#include "boxlog.h"


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

}

MediaManager::MediaManager(QString pathDir, CHANNEL_TYPE type) :
    mMediaModel(nullptr),
    mType(type)
{
    if( pathDir.right(1) != "/" )
        pathDir += "/";
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
    applySnapshot(MediaImportService::scanDirectory(mDirMediaFiles.absolutePath(), libraryFormats()));
}

void MediaManager::applySnapshot(QList<MediaData> snapshot)
{
    if (mMediaModel)
        mMediaModel->beginCollect();
    mMediaList = std::move(snapshot);
    calculateTotalLength(mMediaList);
    if (mMediaModel)
        mMediaModel->endCollect();
}

QStringList MediaManager::libraryFormats() const
{
    if (mType == MUSIC)
        return Settings::allAudioFormats();
    if (mType == VIDEO)
        return Settings::allVideoFormats();
    return Settings::allFormats();
}

QStringList MediaManager::importFormats() const
{
    Settings settings;
    if (mType == MUSIC)
        return settings.availablelAudioFileFormats();
    if (mType == VIDEO)
        return settings.availablelVideoFileFormats();
    return settings.availablelAllFileFormats();
}

void MediaManager::bindDirectory(const QString &name)
{
    mDirMediaFiles.setPath(QDir(mChannelDir).filePath(name));
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
