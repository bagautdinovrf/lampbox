

#ifndef MEDIAMANAGER_H
#define MEDIAMANAGER_H


#include "mediadata.h"
#include "lampdata.h"


#include <QModelIndex>
#include <QScopedPointer>
#include <QDir>


class MediaModel;

using namespace LampBox;

class MediaManager
{
public:
    MediaManager(CHANNEL_TYPE type);
    MediaManager( QString pathDir, CHANNEL_TYPE type );
    ~MediaManager();


public:
    /**
     * @brief collectMediaFiles
     */
    void collectMediaFiles();

    void setDirName( const QString &name );

    /**
     * @brief count         - Количество файлов
     * @return
     */
    int count();

public:

    void setMediaModel(MediaModel *model);

    bool renameChannelDir( const QString &name );

    bool deleteChannel();

    bool createChannelDir( const QString &name );

    /**
     * @brief getMusicData
     * @param id
     * @return
     */
    const MediaData &mediaData(int id);

    /**
     * @brief columnCount
     * @return
     */
    int columnCount();

    /**
     * @brief mediaCont
     * @return
     */
    int mediaCount();

    /**
     * @brief getDirMediaFiles
     * @return
     */
    QDir getDirMediaFiles();

    /**
     * @brief delFile
     * @param fileName
     * @return
     */
    bool delFile(const QString &fileName);

    /**
     * @brief delFile
     * @param num
     * @return
     */
    bool delFile(int num);

    /**
     * @brief addFile
     * @param fileName
     * @return
     */
    bool addFile( const QString &fileName );

    /**
     * @brief totalLength
     * @return
     */
    QString totalLength();
    QString calculateLength( uint length );

private:
    /**
     * @brief setDirMediaFiles
     * @param name
     */
    void setDirMediaFiles(const QString &name);

    void createTagFile( const QString &fileName );

    /**
     * @brief calculateTotalLength
     * @param list
     * @return
     */
    uint calculateTotalLength( const QList<MediaData> &list );

private:
    /// /
    QList<MediaData>                mMediaList;
    /// Имя канала
    QString                         mDirName;
    ///
    QDir                            mDir;
    /// Путь к файлам канала
    QDir                            mDirMediaFiles;
    /// Модель
    MediaModel                      *mMediaModel;
    /// Общая длина всех треков
    uint                            mTotalLength;

    CHANNEL_TYPE              mType;

    QString                         mChannelDir;
};

#endif // MEDIAMANAGER_H
