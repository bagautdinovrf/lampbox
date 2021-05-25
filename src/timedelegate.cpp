

#include "timedelegate.h"


#include <QTimeEdit>
#include <QDebug>


TimeDelegate::TimeDelegate(QWidget *parent, QString timeFotmat)
    : QItemDelegate(parent),
      mTimeFormat(timeFotmat)
{
    //
}

TimeDelegate::~TimeDelegate()
{
    //
}

/**
 * @brief TimeDelegate::createEditor
 * @param parent
 * @return
 */
QWidget *TimeDelegate::createEditor( QWidget *parent, const QStyleOptionViewItem& /* option */, const QModelIndex& /* index */) const
{
    QTimeEdit *editor = new QTimeEdit(parent);
    editor->installEventFilter(const_cast<TimeDelegate*>(this));
    return editor;
}

/**
 * @brief TimeDelegate::setEditorData - запихнуть
 * @param editor
 * @param index
 */
void TimeDelegate::setEditorData( QWidget *editor, const QModelIndex &index) const
{
    QString value = index.data(Qt::DisplayRole).toString();
//    qDebug() << "Время=" << value;
    QTimeEdit *de = static_cast<QTimeEdit*>(editor);
    de->setTime( QTime().fromString(value, mTimeFormat) );
}

/**
 * @brief TimeDelegate::setModelData - выпихнуть
 * @param editor
 * @param model
 * @param index
 */
void TimeDelegate::setModelData( QWidget *editor, QAbstractItemModel *model, const QModelIndex& index) const
{
    QTimeEdit *te = static_cast<QTimeEdit*>(editor);
//    te->interpretText();
    QTime value = te->time();
    model->setData(index, value);
}

/**
 * @brief TimeDelegate::updateEditorGeometry
 * @param editor
 * @param option
 */
void TimeDelegate::updateEditorGeometry( QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex& /* index */) const
{
    editor->setGeometry(option.rect);
}
