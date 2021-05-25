
#include "mediadata.h"


MediaData::MediaData()
{
    ///
}


MediaData::~MediaData()
{
    //
}


////////////////////////////////////////////////////////
// SET - GET
////////////////////////////////////////////////////////
/**
 * @brief MusicData::setFileName
 * @param fileName
 */
void MediaData::setFileName(const QString fileName)
{
    mFileName = fileName;
}

/**
 * @brief MusicData::setFileSize
 * @param size
 */
void MediaData::setFileSize(quint64 size)
{
    mFileSize = size;
}

/**
 * @brief MusicData::getFileName
 * @return
 */
QString MediaData::fileName() const
{
    return mFileName;
}

/**
 * @brief MusicData::getFileSize
 * @return
 */
int MediaData::fileSize() const
{
    return mFileSize;
}

void MediaData::setArtist(const QString &artist)
{
    mArtist = artist;
}

void MediaData::setAlbum(const QString &album)
{
    mAlbum = album;
}

void MediaData::setGenre(const QString &genre)
{
    mGenre = genre;
}

void MediaData::setTitle(const QString &title)
{
    mTitle = title;
}

void MediaData::setLength(uint length)
{
    mLength = length;
}

void MediaData::setYear(uint year)
{
    mYear = year;
}


uint MediaData::length() const
{
    return mLength;
}

QString MediaData::album() const
{
    return mAlbum;
}

QString MediaData::artist() const
{
    return mArtist;
}

QString MediaData::genre() const
{
    return mGenre;
}

QString MediaData::title() const
{
    return mTitle;
}

uint MediaData::year() const
{
    return mYear;
}

