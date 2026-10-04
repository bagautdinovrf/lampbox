
#pragma once
#ifndef CHANNELMODEL_H
#define CHANNELMODEL_H


#include "channelmanager.h"

#include <QAbstractTableModel>


class ChannelModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Role { PlaybackOrderRole = Qt::UserRole + 1, UntilDayOffsetRole };
    explicit ChannelModel(ChannelManager *channelManager, QObject *parent = nullptr);
    ~ChannelModel() override;

    /**
     */
    QVariant data( const QModelIndex& index, int nRole ) const override;

    /**
     * @brief setData
     * @param index
     * @param value
     * @param role
     * @return
     */
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    bool setRule(int row, const QVariantList &fields);
    QString lastError() const;

    /**
      */
    int rowCount(const QModelIndex &parent = QModelIndex() ) const override;

    /**
      */
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
      */
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    /**
      */
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void beginCollect();
    void endCollect();

private:
    ChannelManager *mChannelManager_;
    
signals:
    
public slots:
    
};

#endif // CHANNELMODEL_H
