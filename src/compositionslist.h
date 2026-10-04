#pragma once

#ifndef COMPOSITIONSLIST_H
#define COMPOSITIONSLIST_H
#include <QList>
#include <QAbstractTableModel>
#include "composition.h"
class CompositionsList : public QAbstractTableModel
{
     Q_OBJECT
public:


    CompositionsList(QObject *parent = nullptr);



    int columnCount(const QModelIndex & parent) const override ;

    bool Init(QString fileName);

    int rowCount(const QModelIndex & parent) const override ;


    QVariant data(const QModelIndex & index, int role) const override ;

    QVariant getData(int num, int position) const;

    QVariant headerData(int section, Qt ::Orientation orientation, int role) const override ;
    //Composition& getComposition(const QModelIndex & index) const ;

   QList<Composition> mListOfCompositions;
   void setCompositionName(QString );

};

#endif // COMPOSITIONSLIST_H
