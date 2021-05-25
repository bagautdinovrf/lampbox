#include "datedelegate.h"
#include <QPainter>
#include <QDebug>
DateDelegate::DateDelegate(int startDateColumn, QObject *parent) :
    QItemDelegate(parent),pixmap(":/ico/res/attention.png")
{
    this->mColumn = startDateColumn;
}
QWidget* DateDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const{
    Q_UNUSED(option);
    Q_UNUSED(index);
    QDateEdit *editor= new  QDateEdit (parent);
    editor->installEventFilter(const_cast<DateDelegate*>(this));
    editor->setDate(QDate::currentDate());
    return editor;
}

void DateDelegate::setEditorData( QWidget *editor, const QModelIndex &index) const {
    QDate value = index.model()->data(index, Qt::EditRole).toDate();
    QDateEdit *de = static_cast<QDateEdit*>(editor);
    de->setCalendarPopup(true);
    de->setDate(value);
}

void DateDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                const QModelIndex &index) const
{
    QDateEdit *dateEdit = qobject_cast<QDateEdit *>(editor);
    QDate date = dateEdit->date();
    model->setData(index, date);
    emit setDataMultiple(date);

}
void DateDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QDate dt = QDate::fromString(index.data().toString(), Qt::ISODate);
    QString time;
    time = dt.toString("dd.MM.yyyy");

    painter->save();
    painter->setClipRect(option.rect);

    drawBackground(painter, option, index);
    drawDisplay(painter, option, option.rect, time);
    if(mColumn == index.column() && dt < QDate::currentDate()){
        QRect iconRect = QRect(option.rect.right()-pixmap.width(),option.rect.top(),pixmap.width(),option.rect.height());
        drawDecoration(painter,option,iconRect,pixmap);
    }

    drawFocus(painter, option, option.rect);

    painter->restore();
}
void DateDelegate::commitAndCloseEditor(){
    QDateEdit *editor = qobject_cast<QDateEdit *>(sender());
    emit commitData(editor);
    emit closeEditor(editor);
}
