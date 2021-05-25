#ifndef DELEGATEMONTHEDIT_H
#define DELEGATEMONTHEDIT_H

#include <QObject>
#include <QStyledItemDelegate>
#include "delegatemonthwidget.h"
#include "types.h"

//#include <QLMCommons>
//#include <QLMDelegates>

namespace lampproject{
    namespace delegate {
        class DelegateMonthEdit : public QStyledItemDelegate
        {
            Q_OBJECT
        public:
            DelegateMonthEdit(QObject *parent = 0);
            QString displayText( const QVariant & value, const QLocale & locale ) const;
            QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const;

            void setEditorData(QWidget *editor, const QModelIndex &index) const;
            void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const;

            void updateEditorGeometry(QWidget *editor,
                const QStyleOptionViewItem &option, const QModelIndex &index) const;
            QList<lampproject::Month> getMonth(QString value) const;
        };
}
}

#endif // DELEGATEMONTHEDIT_H
