#ifndef ADVERTMODEL_H
#define ADVERTMODEL_H


#include "advertmanager.h"

#include <QAbstractTableModel>


class AdvertModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit AdvertModel(AdvertManager *advertManager, QObject *parent = 0);

    QVariant data( const QModelIndex& index, int nRole ) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    int rowCount(const QModelIndex &parent = QModelIndex() ) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

private:
    AdvertManager *mAdvertManager;

private slots:
    /// Начало изменения данных модели
    void beginReset();

    /// Конец изменений данных модели
    void endReset();
};

#endif // ADVERTMODEL_H
