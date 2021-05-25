#ifndef DATEDELEGATE_H
#define DATEDELEGATE_H
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QItemDelegate>
#include <QDateEdit>
#include <QCalendarWidget>
#include <QLineEdit>
#include <QRegExp>
#include <QMessageBox>
#define MAX_FREQUENCY 5
#define MIN_FREQUENCY 0

class DateDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    DateDelegate(int frequencyColumn, QObject *parent=0);

    void setEditorData(QWidget *editor, const QModelIndex &index) const;

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;

    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const;
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const;
signals:
    void setDataMultiple(QDate value) const;
private:
    QPixmap pixmap;
private slots:
    void commitAndCloseEditor();
private :
    int mColumn;
};




#endif // DATEDELEGATE_H
