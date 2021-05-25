

#ifndef SIMPLE_LINEEDITDELEGATE_H
#define SIMPLE_LINEEDITDELEGATE_H


#include <QItemDelegate>

namespace lampbox {

class SimpleLineEditDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    explicit SimpleLineEditDelegate(QObject *parent = 0, QString validator = "\\w+");
    ~SimpleLineEditDelegate();

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    
private:
    ///
    QString                                 mValidator;
    
};

}

#endif // SIMPLE_LINEEDITDELEGATE_H
