#include "dateeditdelegate.h"

#include <QDateEdit>

DateEditDelegate::DateEditDelegate(QObject *parent) :
    QItemDelegate(parent)
{
    //
}

DateEditDelegate::~DateEditDelegate()
{
    //
}

QWidget *DateEditDelegate::createEditor( QWidget *parent, const QStyleOptionViewItem& /* option */, const QModelIndex& /* index */) const
{
    QDateEdit *editor = new QDateEdit(parent);
    editor->installEventFilter(const_cast<DateEditDelegate*>(this));
    editor->setCalendarPopup(true);
    editor->setDate(QDate::currentDate());
    return editor;
}

void DateEditDelegate::setEditorData( QWidget *editor, const QModelIndex &index) const
{
    QDate value = index.data(Qt::DisplayRole).toDate();
    QDateEdit *de = static_cast<QDateEdit*>(editor);
    de->setDate(value);
}

void DateEditDelegate::setModelData( QWidget *editor, QAbstractItemModel *model, const QModelIndex& index) const
{
    QDateEdit *de = static_cast<QDateEdit*>(editor);
    QDate value = de->date();
    model->setData(index, value);
}

//void DateEditDelegate::updateEditorGeometry( QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex& /* index */) const
//{
//    editor->setGeometry(option.rect);
//}
