

#include "simplelineeditdelegate.h"


#include <QLineEdit>
#include <QDebug>
#include <QRegExpValidator>

using namespace lampbox;

SimpleLineEditDelegate::SimpleLineEditDelegate(QObject *parent, QString validator) :
    QItemDelegate(parent),
    mValidator(validator)
{
    //
}

SimpleLineEditDelegate::~SimpleLineEditDelegate()
{
    //
}

QWidget *SimpleLineEditDelegate::createEditor( QWidget *parent, const QStyleOptionViewItem& /* option */, const QModelIndex& /* index */) const
{
    QLineEdit *editor = new QLineEdit(parent);
//    QRegExp rx("\\w+");
    QRegExp rx( mValidator );
    QValidator *valid = new QRegExpValidator(rx, editor);
    editor->setValidator( valid );
    editor->setMaxLength(15);
    editor->installEventFilter(const_cast<SimpleLineEditDelegate*>(this));
    return editor;
}

void SimpleLineEditDelegate::setEditorData( QWidget *editor, const QModelIndex &index) const
{
    QString value = index.data(Qt::DisplayRole).toString();
    QLineEdit *le = static_cast<QLineEdit*>(editor);
    le->setText(value);
}

void SimpleLineEditDelegate::setModelData( QWidget *editor, QAbstractItemModel *model, const QModelIndex& index) const
{
    QLineEdit *le = static_cast<QLineEdit*>(editor);
    QString value = le->text();
//    if( value.isEmpty() )
//        return;
    model->setData(index, value);
}

void SimpleLineEditDelegate::updateEditorGeometry( QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex& /* index */) const
{
    editor->setGeometry(option.rect);
}
