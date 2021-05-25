#include "compositionslist.h"

CompositionsList::CompositionsList(QObject *parent) : QAbstractTableModel(parent)
{
    //mListOfCompositions = new QList<Composition>();

}

int CompositionsList::columnCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent);
    return 2;
}

int CompositionsList::rowCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent);
    return mListOfCompositions.size();
}

QVariant CompositionsList::data(const QModelIndex & index, int role) const {
    if (index.isValid() && role == Qt ::DisplayRole)
        return getData(index.row(), index.column());
    return QVariant ();
}

QVariant CompositionsList::getData(int num, int position) const {
    switch (position) {
        case 0:
            return QVariant (mListOfCompositions.at(num).getCompositionName());
        case 1:
            return QVariant (mListOfCompositions.at(num).getFrequency());
        default:
            return QVariant ();
    }
}

QVariant CompositionsList::headerData(int section, Qt ::Orientation orientation, int role) const {
    if (role != Qt ::DisplayRole)
        return QVariant ();
    if (orientation == Qt ::Vertical)
        return QVariant (section + 1);
    else
        switch (section) {
        case 0:
            return QVariant (trUtf8("Название композиции"));
        case 1:
            return QVariant (trUtf8("Частота звучания"));
        default:
            return QVariant ();
        }
}
//Composition& CompositionsList::getComposition(const QModelIndex & index) const {
  //  return mListOfCompositions[index.row()];
//}

bool CompositionsList::Init(QString fileName){
    Parser csv;
    if( csv.Init(fileName)){

        qDebug()<<"file inift";
    }
    else{

        qDebug()<<"not open";
        return false;
    }
    if ( csv.Parse()){
        qDebug()<<"Parsered ok!";
    }
    else{
        qDebug()<<"parser failed!";
    }

    for(int i =0; i<csv.mList.size(); i++){
        Composition cmp1;

        cmp1.setCompositionName(csv.mList[i][0]);
        cmp1.setFrequency(csv.mList[i][1].toInt());
        mListOfCompositions.append(cmp1);
    };

    return true;
}
