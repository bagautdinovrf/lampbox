#ifndef RADIOBUTTONDELEGATE_H
#define RADIOBUTTONDELEGATE_H

#include <QTableView>
#include <QStyledItemDelegate>

class QMouseEvent;
class QKeyEvent;

static const int RadioSelectedRole = Qt::UserRole + 102;
static const int IsRadioRole = Qt::UserRole + 103;

class RadioButtonItemDelegate : public QStyledItemDelegate
{
public:
    RadioButtonItemDelegate(QObject *p);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

#endif // RADIOBUTTONDELEGATE_H
