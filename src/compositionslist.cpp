#include "compositionslist.h"
#include <QFile>
#include <QTextStream>

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
            return QVariant (tr("Название композиции"));
        case 1:
            return QVariant (tr("Частота звучания"));
        default:
            return QVariant ();
        }
}
//Composition& CompositionsList::getComposition(const QModelIndex & index) const {
  //  return mListOfCompositions[index.row()];
//}

bool CompositionsList::Init(QString fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    QList<Composition> compositions;
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty())
            continue;
        const QStringList fields = line.split(QLatin1Char(';'));
        // Historical reports may append extra columns; only the first two
        // belong to the manager's composition/frequency contract.
        if (fields.size() < 2 || fields.at(0).trimmed().isEmpty())
            return false;
        bool valid = false;
        const int frequency = fields.at(1).toInt(&valid);
        if (!valid || frequency < 0)
            return false;
        Composition composition;
        composition.setCompositionName(fields.at(0));
        composition.setFrequency(frequency);
        compositions.append(composition);
    }
    if (stream.status() != QTextStream::Ok)
        return false;
    beginResetModel();
    mListOfCompositions = std::move(compositions);
    endResetModel();
    return true;
}
