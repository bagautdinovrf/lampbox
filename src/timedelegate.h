

#ifndef QTIMEDELEGATE_H
#define QTIMEDELEGATE_H


#include <QItemDelegate>


class TimeDelegate : public QItemDelegate
{
public:
    TimeDelegate( QWidget *parent=nullptr, QString timeFotmat = "HH:mm");
    ~TimeDelegate() override;

    /**
     * @brief createEditor
     * @param parent
     * @param option
     * @param index
     * @return
     */
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief setEditorData
     * @param editor
     * @param index
     */
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    /**
     * @brief setModelData
     * @param editor
     * @param model
     * @param index
     */
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    /**
     * @brief updateEditorGeometry
     * @param editor
     * @param option
     * @param index
     */
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;


private:
    const QString             mTimeFormat;
};

#endif // QTIMEDELEGATE_H
