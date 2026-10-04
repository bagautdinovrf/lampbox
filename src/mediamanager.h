

#pragma once
#ifndef MEDIAMANAGER_H
#define MEDIAMANAGER_H


#include "mediadata.h"
#include "lampdata.h"


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

    void bindDirectory(const QString &name);
    void applySnapshot(QList<MediaData> snapshot);
    const QList<MediaData> &snapshot() const { return mMediaList; }
    QStringList libraryFormats() const;
    QStringList importFormats() const;

    /**
     * @brief count         - Количество файлов
     * @return
     */
    int count();

public:

    void setMediaModel(MediaModel *model);

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
     * @brief totalLength
     * @return
     */
    QString totalLength();
    QString calculateLength( uint length );

private:
    /**
     * @brief calculateTotalLength
     * @param list
     * @return
     */
    uint calculateTotalLength( const QList<MediaData> &list );

private:
    /// /
    QList<MediaData>                mMediaList;
    /// Путь к файлам канала
    QDir                            mDirMediaFiles;
    /// Модель
    MediaModel                      *mMediaModel;
    /// Общая длина всех треков
    uint                            mTotalLength = 0;

    CHANNEL_TYPE              mType;

    QString                         mChannelDir;
};

#endif // MEDIAMANAGER_H
