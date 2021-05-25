#ifndef ONAIRDELEGATE_H
#define ONAIRDELEGATE_H

#include <QStyledItemDelegate>
namespace lampproject{
    namespace delegate {
        class OnAirDelegate : public QStyledItemDelegate
        {
            Q_OBJECT
        public:
            OnAirDelegate(QObject *parent = 0);
            QString displayText( const QVariant & value, const QLocale & locale ) const;

            QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const;

            void setEditorData(QWidget *editor, const QModelIndex &index) const;
            void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const;
            void updateEditorGeometry(QWidget *editor,
                const QStyleOptionViewItem &option, const QModelIndex &index) const;
            void paint( QPainter * painter, const QStyleOptionViewItem & option,
                        const QModelIndex & index ) const;
        signals:
            void setDataMultiple(int value) const;
        public slots:

        };
    }
}
#endif // ONAIRDELEGATE_H
