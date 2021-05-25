
#ifndef CHANNELMODEL_H
#define CHANNELMODEL_H


#include "channelmanager.h"

#include <QAbstractTableModel>


class ChannelModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ChannelModel(ChannelManager *channelManager, QObject *parent = 0);
    ~ChannelModel();

    /**
     */
    QVariant data( const QModelIndex& index, int nRole ) const;

    /**
     * @brief setData
     * @param index
     * @param value
     * @param role
     * @return
     */
    bool setData(const QModelIndex &index, const QVariant &value, int role);

    /**
      */
    int rowCount(const QModelIndex &parent = QModelIndex() ) const;

    /**
      */
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    /**
      */
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    /**
      */
    Qt::ItemFlags flags(const QModelIndex &index) const;

    void beginCollect();
    void endCollect();

private:
    ChannelManager *mChannelManager_;
    
signals:
    
public slots:
    
};

#endif // CHANNELMODEL_H
