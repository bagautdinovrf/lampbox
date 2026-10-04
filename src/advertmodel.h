#pragma once
#ifndef ADVERTMODEL_H
#define ADVERTMODEL_H


#include "advertmanager.h"

#include <QAbstractTableModel>


class AdvertModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum { CompiledMinutesRole = Qt::UserRole + 100 };
    explicit AdvertModel(AdvertManager *advertManager, QObject *parent = nullptr);

    QVariant data( const QModelIndex& index, int nRole ) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    bool setRule(int row, const QVariantList &fields);
    QString lastError() const;
    int rowCount(const QModelIndex &parent = QModelIndex() ) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    AdvertManager *mAdvertManager;

private slots:
    /// Начало изменения данных модели
    void beginReset();

    /// Конец изменений данных модели
    void endReset();
};

#endif // ADVERTMODEL_H
