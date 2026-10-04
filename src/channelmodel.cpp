

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
                return QVariant( tr("Плейлист") );
            case 1:
                return QVariant( tr("Начало") );
            case 2:
                return QVariant( tr("Окончание") );
            case 3:
                return QVariant( tr("Дни недели") );
            case 4:
                return QVariant( tr("Дни") );
            case 5:
                return QVariant( tr("Месяцы") );
            case 6:
                return QVariant( tr("Громкость") );
            default:
                return QVariant();
        }
}

QVariant ChannelModel::data(const QModelIndex &index, int nRole) const
{
//    qDebug() << Q_FUNC_INFO;
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount() || index.column() >= columnCount())
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
    if (!index.isValid() || index.model() != this || role != Qt::EditRole
            || index.row() >= rowCount() || index.column() < 0 || index.column() >= 7)
        return false;
    QVariantList fields;
    for (int column = 0; column < 7; ++column)
        fields.append(data(this->index(index.row(), column), Qt::EditRole));
    fields[index.column()] = value;
    return setRule(index.row(), fields);
}

bool ChannelModel::setRule(int row, const QVariantList &fields)
{
    if (!mChannelManager_->setRule(row, fields)) return false;
    emit dataChanged(index(row, 0), index(row, 6), {Qt::DisplayRole, Qt::EditRole});
    return true;
}

QString ChannelModel::lastError() const { return mChannelManager_->lastError(); }
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
