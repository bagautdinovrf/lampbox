

#ifndef MEDIAMODEL_H
#define MEDIAMODEL_H

#include "lampdata.h"

#include <QAbstractTableModel>


//Qt
class QMimeData;

//LampBox
class MediaManager;

using namespace LampBox;

class MediaModel : public QAbstractTableModel
{
    Q_OBJECT
public:

    explicit MediaModel(MediaManager *mediaManager, CHANNEL_TYPE type, QObject *parent = 0);
    ~MediaModel();

    void setMediaManager(MediaManager * mediaManager);
    void setManagerType(CHANNEL_TYPE type);

    CHANNEL_TYPE type();

    QVariant data( const QModelIndex& index, int nRole ) const;
    int rowCount(const QModelIndex &parent = QModelIndex() ) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;

    void beginCollect();
    void endCollect();

signals:
    void dropFileList( const QStringList &files );


private:
    MediaManager                        *mMediaManager_;
    CHANNEL_TYPE                        mManagerType;

};

#endif // MEDIAMODEL_H
