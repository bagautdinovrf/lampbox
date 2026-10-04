
#include "advertmodel.h"
#include "stationmanager.h"


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
                return QVariant( tr("Название") );
            case 1:
                return QVariant( tr("Часы") );
            case 2:
                return QVariant( tr("Минуты/Частота") );
            case 3:
                return QVariant( tr("Дни недели") );
            case 4:
                return QVariant( tr("Начало") );
            case 5:
                return QVariant( tr("Окончание") );
            case 6:
                return QVariant( tr("Громкость") );
            default:
                return QVariant();
        }
}

/**
  */
QVariant AdvertModel::data(const QModelIndex &index, int nRole) const
{
//    qDebug() << Q_FUNC_INFO;
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount() || index.column() >= columnCount())
        return QVariant();

    if (nRole == CompiledMinutesRole) {
        QVariantList minutes;
        for (int minute : mAdvertManager->compiledMinutes(index.row())) minutes.append(minute);
        return minutes;
    }
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
    if (!index.isValid() || index.model() != this || role != Qt::EditRole
            || index.row() >= rowCount() || index.column() <= 0 || index.column() >= 7
            || !(flags(index) & Qt::ItemIsEditable)) return false;
    QVariantList fields;
    for (int column = 0; column < 7; ++column)
        fields.append(data(this->index(index.row(), column), Qt::EditRole));
    fields[index.column()] = value;
    return setRule(index.row(), fields);
}

bool AdvertModel::setRule(int row, const QVariantList &fields)
{
    if (!mAdvertManager->setRule(row, fields)) return false;
    emit dataChanged(index(row, 0), index(row, 6), {Qt::DisplayRole, Qt::EditRole});
    return true;
}

QString AdvertModel::lastError() const { return mAdvertManager->lastError(); }
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
        flags &= ~Qt::ItemIsEditable;

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
