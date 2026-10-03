#ifndef VOLUMEDELEGATE_H
#define VOLUMEDELEGATE_H

#include <QItemDelegate>

class VolumeDelegate : public QItemDelegate
{
public:
    VolumeDelegate( QWidget *parent=nullptr );

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // VOLUMEDELEGATE_H
