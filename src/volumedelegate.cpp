
#include "volumedelegate.h"

#include <QSpinBox>

VolumeDelegate::VolumeDelegate(QWidget *parent) :
    QItemDelegate(parent)
{
    //
}

QWidget *VolumeDelegate::createEditor( QWidget *parent, const QStyleOptionViewItem& /* option */, const QModelIndex& /* index */) const
{
    QSpinBox *editor = new QSpinBox(parent);
    editor->setMaximum(0);
    editor->setMaximum(100);
    editor->installEventFilter(const_cast<VolumeDelegate*>(this));
    return editor;
}

void VolumeDelegate::setEditorData( QWidget *editor, const QModelIndex &index) const
{
    int value = index.data(Qt::DisplayRole).toInt();
    QSpinBox *sb = static_cast<QSpinBox*>(editor);
    sb->setValue(value);
}

void VolumeDelegate::setModelData( QWidget *editor, QAbstractItemModel *model, const QModelIndex& index) const
{
    QSpinBox *sb = static_cast<QSpinBox*>(editor);
    sb->interpretText();
    model->setData(index, sb->value());
}

void VolumeDelegate::updateEditorGeometry( QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex& /* index */) const
{
    editor->setGeometry(option.rect);
}
