
#include "mediamodel.h"
#include "mediamanager.h"

#include <QMimeData>
#include <QColor>
#include <QUrl>
#include <QDebug>


MediaModel::MediaModel(MediaManager *mediaManager, CHANNEL_TYPE type, QObject *parent) :
    QAbstractTableModel(parent),
    mMediaManager_(mediaManager),
    mManagerType(type)
{
    if(parent)
        connect( this, SIGNAL(dropFileList( QStringList )), parent, SLOT(copyFiles( QStringList ) ));
}

MediaModel::~MediaModel()
{
//    qDebug() << Q_FUNC_INFO;
}

void MediaModel::setMediaManager(MediaManager *mediaManager)
{
    beginCollect();
    mMediaManager_ = mediaManager;
    if(mediaManager)
        mMediaManager_->setMediaModel(this);
    endCollect();
}

/**
  */
QVariant MediaModel::headerData(int section, Qt::Orientation orientation, int nRole ) const
{
//    qDebug() << Q_FUNC_INFO;
    if( nRole != Qt::DisplayRole )
        return QVariant();

    if (orientation == Qt::Vertical)
        return QVariant(section + 1);
    else
        switch (section)
        {
            case 0:
                 if( mMediaManager_) {
                     QString name = mMediaManager_->getDirMediaFiles().dirName();
                     if( ADVERT == mManagerType )
                         name = "Реклама";
                     return QVariant( name );
                 }
                return QVariant( trUtf8("Файлы") );
            default:
                return QVariant();
        }
    return QVariant();
}

/**
  */
QVariant MediaModel::data(const QModelIndex &index, int nRole) const
{
//    qDebug() << Q_FUNC_INFO;
    if( !index.isValid() || !mMediaManager_)
        return QVariant();

    if( nRole == Qt::DisplayRole ) {
        switch( index.column() )
        {
            case 0:
                return QVariant( mMediaManager_->mediaData(index.row()).fileName() );
            default:
                return QVariant();
        }
    } else if( nRole == Qt::BackgroundColorRole ) {
        switch( index.row() % 2 ) {
        case 1:
            // Snow3
//            return QVariant(QColor(205, 201, 201) );
            return QVariant(QColor(220, 220, 220) );
        default:
           return QVariant();
        }
    }
    return QVariant();
}

/**
  */
int MediaModel::rowCount(const QModelIndex &) const
{
//    qDebug() << Q_FUNC_INFO;
    if(!mMediaManager_)
        return 0;
//    qDebug() << Q_FUNC_INFO;
    return mMediaManager_->mediaCount();
}

/**
  */
int MediaModel::columnCount(const QModelIndex &) const
{
//    qDebug() << Q_FUNC_INFO;
    if(!mMediaManager_)
        return 1; //NOTE было 0
//    qDebug() << Q_FUNC_INFO;
    return mMediaManager_->columnCount();
}

/**
  */
Qt::ItemFlags MediaModel::flags(const QModelIndex &index) const
{
//    qDebug() << Q_FUNC_INFO;
    Qt::ItemFlags flags = QAbstractTableModel::flags(index) |  Qt::ItemIsDropEnabled;
    return index.isValid() ? ( flags | Qt::ItemIsEnabled | Qt::ItemIsSelectable /*| Qt::ItemIsEditable*/ ) : flags;
}

void MediaModel::beginCollect()
{
    beginResetModel();
}

void MediaModel::endCollect()
{
    endResetModel();
}

void MediaModel::setManagerType(CHANNEL_TYPE type)
{
    mManagerType = type;
}

CHANNEL_TYPE MediaModel::type()
{
    return mManagerType;
}

Qt::DropActions MediaModel::supportedDropActions() const
{
//qDebug() << Q_FUNC_INFO;
    return Qt::CopyAction;
}

QStringList MediaModel::mimeTypes() const
{
    QStringList types;
    types << "text/uri-list";
    return types;
}

bool MediaModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const
{
//qDebug() << Q_FUNC_INFO;
    Q_UNUSED(action);
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);
    return data->hasUrls();
 }

bool MediaModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
//qDebug() << Q_FUNC_INFO;
    Q_UNUSED(action);
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);
    if( !data->hasUrls() )
        return false;

    QStringList files;
    foreach ( QUrl url, data->urls() ) {
       files <<  url.toLocalFile();
    }
    emit dropFileList(files);
    return true;
 }
