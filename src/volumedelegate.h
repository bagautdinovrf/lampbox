#ifndef VOLUMEDELEGATE_H
#define VOLUMEDELEGATE_H

#include <QItemDelegate>

class VolumeDelegate : public QItemDelegate
{
public:
    VolumeDelegate( QWidget *parent=0 );

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

#endif // VOLUMEDELEGATE_H
