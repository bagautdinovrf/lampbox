#include "frequencydelegate.h"

FrequencyDelegate::FrequencyDelegate(int fr, QObject *parent) :
    QItemDelegate(parent)
{
    this->mColumn = fr;
}

QWidget* FrequencyDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const{
    Q_UNUSED(option);
    Q_UNUSED(index);
    FrEditor *editor= new  FrEditor (parent);
    editor->installEventFilter(const_cast<FrequencyDelegate*>(this));
    editor->setText("0");
    return editor;
}

void FrequencyDelegate::setEditorData(
        QWidget *editor,
        const QModelIndex &index) const {
    QString value = index.model()->data(
                index, Qt::EditRole).toString();

    FrEditor* de = static_cast<FrEditor*>(editor);

    de->setText(value);
}

void FrequencyDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                     const QModelIndex &index) const
{
    FrEditor *frEdit = qobject_cast<FrEditor *>(editor);
    QString frequency = frEdit->text();
    model->setData(index, frequency);

}

void FrequencyDelegate::commitAndCloseEditor(){
    FrEditor *editor = qobject_cast<FrEditor *>(sender());
    emit commitData(editor);
    emit closeEditor(editor);
}
