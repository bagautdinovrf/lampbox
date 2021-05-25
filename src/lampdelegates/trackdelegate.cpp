#include "trackdelegate.h"

TrackDelegate::TrackDelegate(int frequencyColumn, QObject *parent) :
    QItemDelegate(parent)
{
       this->mFrequencyColumn = frequencyColumn;
}

QWidget* TrackDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                      const QModelIndex &index) const{
    if(index.column()==mFrequencyColumn){
        qDebug(" delegate %i",index.row());
        QSpinBox *spinEdit= new   QSpinBox(parent);
        spinEdit->setMaximum( MAX_FREQUENCY );
        spinEdit->setMinimum( MIN_FREQUENCY );
        return spinEdit;
    }
    else return QItemDelegate::createEditor(parent, option, index);
}
void TrackDelegate::updateEditorGeometry(QWidget *editor,
             const QStyleOptionViewItem &option, const QModelIndex &/* index */) const
{
    qDebug("delegate");
    editor->setGeometry(option.rect);
}
