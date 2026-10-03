

#ifndef SIMPLE_LINEEDITDELEGATE_H
#define SIMPLE_LINEEDITDELEGATE_H


#include <QItemDelegate>

namespace lampbox {

class SimpleLineEditDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    explicit SimpleLineEditDelegate(QObject *parent = nullptr, QString validator = "\\w+");
    ~SimpleLineEditDelegate() override;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    
private:
    ///
    QString                                 mValidator;
    
};

}

#endif // SIMPLE_LINEEDITDELEGATE_H
