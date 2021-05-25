#ifndef COMPOSITIONSLIST_H
#define COMPOSITIONSLIST_H
#include <QList>
#include <QAbstractTableModel>
#include "composition.h"
#include "parser.h"
class CompositionsList : public QAbstractTableModel
{
     Q_OBJECT
public:


    CompositionsList(QObject *parent = 0);



    int columnCount(const QModelIndex & parent) const ;

    bool Init(QString fileName);

    int rowCount(const QModelIndex & parent) const ;


    QVariant data(const QModelIndex & index, int role) const ;

    QVariant getData(int num, int position) const;

    QVariant headerData(int section, Qt ::Orientation orientation, int role) const ;
    //Composition& getComposition(const QModelIndex & index) const ;

   QList<Composition> mListOfCompositions;
   void setCompositionName(QString );

};

#endif // COMPOSITIONSLIST_H
