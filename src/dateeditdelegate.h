#ifndef DATEEDITDELEGATE_H
#define DATEEDITDELEGATE_H

#include <QItemDelegate>

class DateEditDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    explicit DateEditDelegate(QObject *parent = nullptr);
    ~DateEditDelegate() override;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
//    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // DATEEDITDELEGATE_H
