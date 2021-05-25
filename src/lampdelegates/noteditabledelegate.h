#ifndef NOTEDITABLEDELEGATE_H
#define NOTEDITABLEDELEGATE_H
#include <QItemDelegate>

class NotEditableDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    explicit NotEditableDelegate(QObject *parent = 0)
        : QItemDelegate(parent)
    {}

protected:
    QWidget* createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const
    { return Q_NULLPTR; }

};
#endif // NOTEDITABLEDELEGATE_H
