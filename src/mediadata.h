

#ifndef MUSICDATA_H
#define MUSICDATA_H


#include <QString>
#include <QTime>


class MediaData
{
public:
    MediaData();
    ~MediaData();

    /**
     * @brief setFileName
     * @param fileName
     */
    void setFileName(const QString fileName );

    /**
     * @brief setFileSize
     * @param size
     */
    void setFileSize(quint64 size);

    void setLength( uint length);
    void setAlbum( const QString &album);
    void setArtist( const QString &artist);
    void setTitle( const QString &title);
    void setGenre( const QString &genre);
    void setYear( uint year );

    /**
     * @brief getFileName
     * @return
     */
    QString fileName() const;

    /**
     * @brief getFileSize
     * @return
     */
    int fileSize() const;

    QString title() const;
    QString genre() const;
    QString album() const;
    QString artist() const;
    uint year() const;
    uint length() const;

private:
    /// Наименование файла
    QString                             mFileName;
    /// Размер файла
    int                                 mFileSize;
    /// Продолжительность
    uint                                mLength;
    /// Альбом
    QString                             mAlbum;
    /// Артист
    QString                             mArtist;
    /// Название
    QString                             mTitle;
    /// Жанр
    QString                             mGenre;
    /// Год
    uint                                mYear;
};

#endif // MUSICDATA_H
