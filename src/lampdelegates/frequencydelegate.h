#ifndef FREQUENCYDELEGATE_H
#define FREQUENCYDELEGATE_H

#include <QItemDelegate>
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

#include "types.h"

typedef QLineEdit FrEditor;

class FrequencyDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    FrequencyDelegate(int frequencyColumn, QObject *parent=0);

    void setEditorData(QWidget *editor, const QModelIndex &index) const;

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;

    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const;
private slots:
    void commitAndCloseEditor();
private :
    int mColumn;
};

#endif // FREQUENCYDELEGATE_H



