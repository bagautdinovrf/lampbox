#pragma once



#ifndef MEDIAMODEL_H
#define MEDIAMODEL_H

#include "mediaboxmanagerdata.h"

#include <QAbstractTableModel>
#include <QSet>


//Qt
class QMimeData;

class MediaManager;

using namespace MediaBoxManager;

class MediaModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        FileNameColumn = 0,
        TitleColumn,
        ArtistColumn,
        AlbumColumn,
        DurationColumn,
        FormatColumn,
        ColumnCount
    };

    enum Role {
        FileNameRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        GenreRole,
        YearRole,
        DurationSecondsRole,
        FormatRole,
        FileSizeRole,
        MediaTypeRole,
        SortRole,
        PlayingRole
    };

    explicit MediaModel(MediaManager *mediaManager, CHANNEL_TYPE type, QObject *parent = nullptr);
    ~MediaModel() override;

    void setMediaManager(MediaManager * mediaManager);
    void setManagerType(CHANNEL_TYPE type);
    void setPlayingFiles(const QStringList &paths);

    CHANNEL_TYPE type();

    QVariant data( const QModelIndex& index, int nRole ) const override;
    int rowCount(const QModelIndex &parent = QModelIndex() ) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;

    void beginCollect();
    void endCollect();

signals:
    void dropFileList( const QStringList &files );


private:
    bool isPlaying(const QString &fileName) const;
    QSet<QString> mPlayingFiles;
    MediaManager                        *mMediaManager_;
    CHANNEL_TYPE                        mManagerType;

};

#endif // MEDIAMODEL_H
