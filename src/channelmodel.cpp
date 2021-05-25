

#include "channelmodel.h"


#include <QDebug>

ChannelModel::ChannelModel(ChannelManager *channelManager, QObject *parent) :
    QAbstractTableModel(parent),
    mChannelManager_(channelManager)
{
//    qDebug() << Q_FUNC_INFO;
}

ChannelModel::~ChannelModel()
{
//    qDebug() << Q_FUNC_INFO;
}

QVariant ChannelModel::headerData(int section, Qt::Orientation orientation, int nRole ) const
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
                return QVariant( trUtf8("Плейлист") );
            case 1:
                return QVariant( trUtf8("Начало") );
            case 2:
                return QVariant( trUtf8("Окончание") );
            case 3:
                return QVariant( trUtf8("Дни недели") );
            case 4:
                return QVariant( trUtf8("Дни") );
            case 5:
                return QVariant( trUtf8("Месяцы") );
            case 6:
                return QVariant( trUtf8("Громкость") );
            default:
                return QVariant();
        }
    return QVariant();
}

QVariant ChannelModel::data(const QModelIndex &index, int nRole) const
{
//    qDebug() << Q_FUNC_INFO;
    if( !index.isValid() )
        return QVariant();

    if( nRole == Qt::DisplayRole || nRole == Qt::EditRole)
    {
        int row = index.row();
        switch( index.column() )
        {
            case 0:
                return QVariant( mChannelManager_->channel(row).channelName() );
            case 1:
                return QVariant( mChannelManager_->channel(row).startTime().toString("HH:mm") );
            case 2:
                return QVariant( mChannelManager_->channel(row).endTime().toString("HH:mm") );
            case 3:
                return QVariant( mChannelManager_->channel(row).daysOfWeek() );
            case 4:
                return QVariant( mChannelManager_->channel(row).days() );
            case 5:
                return QVariant( mChannelManager_->channel(row).months() );
            case 6:
                return QVariant( mChannelManager_->channel(row).volume() );
            default:
                return QVariant();
        }
    }
    return QVariant();
}

bool ChannelModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    bool res = true;
    if( index.isValid() && role == Qt::EditRole ) {
        int row = index.row();
        switch( index.column() )
        {
        case 0:
            if ( mChannelManager_->containsChannel( value.toString() ) ) {
                res = false;
            } else {
                if( mChannelManager_->channel(row).mediaManager().renameChannelDir(value.toString()) )
                    mChannelManager_->channel(row).setChannelName(value.toString());
                else
                    res = false;
            }
            break;
        case 1:
            mChannelManager_->channel(row).setStartTime(value.toTime());
            break;
        case 2:
            mChannelManager_->channel(row).setEndTime(value.toTime());
            break;
        case 3:
            mChannelManager_->channel(row).setDaysOfWeek(value.toString());
            break;
        case 4:
            mChannelManager_->channel(row).setDays(value.toString());
            break;
        case 5:
            mChannelManager_->channel(row).setMonths(value.toString());
            break;
        case 6:
            mChannelManager_->channel(row).setVolume(value.toInt());
            break;
        default:
            res = false;
        }
    }

    if( res ) {
        mChannelManager_->saveChannels();
        emit dataChanged(index, index);
    }

    return res;
}

int ChannelModel::rowCount(const QModelIndex &mi) const
{
//    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(mi)
    return mChannelManager_->channelCount();
}

int ChannelModel::columnCount(const QModelIndex &mi) const
{
//    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(mi)
    return mChannelManager_->columnCount();
}

Qt::ItemFlags ChannelModel::flags(const QModelIndex &index) const
{
//    qDebug() << Q_FUNC_INFO;
    Qt::ItemFlags flags = QAbstractTableModel::flags(index);
    if (!index.isValid() )
        return flags;

//    switch( index.column() ) {
//    case 0:
//        flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
//        break;
//    default:
//        flags |= Qt::ItemIsEditable;
//        break;
//    }

    flags |= Qt::ItemIsEditable;
    return flags;
}

void ChannelModel::beginCollect()
{
    beginResetModel();
}

void ChannelModel::endCollect()
{
    endResetModel();
}
