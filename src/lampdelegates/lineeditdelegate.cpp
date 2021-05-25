#include "lineeditdelegate.h"
#include <QDebug>

using namespace lampproject::delegate;

LineEditDelegate::LineEditDelegate(QObject *parent) :
    QStyledItemDelegate(parent)
{

}
LineEditDelegate::~LineEditDelegate(){
}
QString LineEditDelegate::displayText( const QVariant & value, const QLocale & locale ) const{
    return value.toString();
}
void LineEditDelegate::updateEditorGeometry(QWidget *editor,
                                            const QStyleOptionViewItem &option, const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect);
}

QWidget* LineEditDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const{

    QLineEdit *lineEdit= new   QLineEdit(parent);
    //lineEdit->installEventFilter(const_cast<LineEditDelegate*>(this));

    QStringList completerList;
    for(int row = 0 ; row < index.model()->rowCount(); ++row){
        completerList << index.model()->data(index.model()->index(row,index.column())).toString();
    }

    QCompleter *completer = new QCompleter(completerList);
    lineEdit->setCompleter(completer);
    return lineEdit;
}

void LineEditDelegate::setEditorData(QWidget *editor,
                                     const QModelIndex &index) const
{
    QString value = index.model()->data(
                index, Qt::EditRole).toString();
    QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
    lineEdit->setText(value);
}

void LineEditDelegate::commitAndCloseEditor(){
    QLineEdit *lineEdit = qobject_cast<QLineEdit *>(sender());

    emit commitData(lineEdit);
    emit closeEditor(lineEdit);
}

void LineEditDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                    const QModelIndex &index) const
{

    QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
    QString text = lineEdit->text();
    model->setData(index, text);
    emit setDataMultiple(text);
   // model->submit();
}
