
#include "advertmodel.h"


AdvertModel::AdvertModel(AdvertManager *advertManager, QObject *parent) :
    QAbstractTableModel(parent),
    mAdvertManager(advertManager)
{
    //
}


QVariant AdvertModel::headerData(int section, Qt::Orientation orientation, int nRole ) const
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
                return QVariant( trUtf8("Название") );
            case 1:
                return QVariant( trUtf8("Часы") );
            case 2:
                return QVariant( trUtf8("Минуты/Частота") );
            case 3:
                return QVariant( trUtf8("Дни недели") );
            case 4:
                return QVariant( trUtf8("Начало") );
            case 5:
                return QVariant( trUtf8("Окончание") );
            case 6:
                return QVariant( trUtf8("Громкость") );
            default:
                return QVariant();
        }
    return QVariant();
}

/**
  */
QVariant AdvertModel::data(const QModelIndex &index, int nRole) const
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
                return QVariant( mAdvertManager->advert(row).name() );
            case 1:
                return QVariant( mAdvertManager->advert(row).hours() );
            case 2:
                return QVariant( mAdvertManager->advert(row).minuts() );
            case 3:
                return QVariant( mAdvertManager->advert(row).days() );
            case 4:
                return QVariant( mAdvertManager->advert(row).startDate() );
            case 5:
                return QVariant( mAdvertManager->advert(row).endDate() );
            case 6:
                return QVariant( mAdvertManager->advert(row).volume() );
            default:
                return QVariant();
        }
    }
    return QVariant();
}

bool AdvertModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    bool res = true;
    if( index.isValid() && role == Qt::EditRole ) {
        int row = index.row();
        switch( index.column() )
        {
            case 1:
                mAdvertManager->advert(row).setHours( value.toString() );
                break;
            case 2:
                mAdvertManager->advert(row).setMinuts( value.toString() );
                break;
            case 3:
                mAdvertManager->advert(row).setDays( value.toString() );
                break;
            case 4:
                mAdvertManager->advert(row).setStartDate( value.toDate() );
                break;
            case 5:
                mAdvertManager->advert(row).setEndDate( value.toDate() );
                break;
            case 6:
                mAdvertManager->advert(row).setVolume( value.toInt() );
                break;

            default:
                res = false;
        }
    }

    if( res ) {
        mAdvertManager->saveAdvert();
        emit dataChanged(index, index);
    }

    return res;
}

/**
  */
int AdvertModel::rowCount(const QModelIndex &mi) const
{
//    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(mi)
    return mAdvertManager->count();
}

/**
  */
int AdvertModel::columnCount(const QModelIndex &parent) const
{
//    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(parent)
    return mAdvertManager->column();
}

/**
  */
Qt::ItemFlags AdvertModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractTableModel::flags(index);
    if (!index.isValid() )
        return flags;

    if( index.column() != 0 )
        flags |= Qt::ItemIsEditable;

    if( STATION_NETWORK == StationManager::Instance().type() )
        flags ^= Qt::ItemIsEditable;

    return flags;
}

void AdvertModel::beginReset()
{
    beginResetModel();
}

void AdvertModel::endReset()
{
    endResetModel();
}
