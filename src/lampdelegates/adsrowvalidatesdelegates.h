#ifndef ADSROWVALIDATESDELEGATES_H
#define ADSROWVALIDATESDELEGATES_H

#include <QWidget>
#include <QStyledItemDelegate>

namespace lampproject{
    namespace delegate {

        class AdsRowValidatesDelegates : public QStyledItemDelegate
        {
            Q_OBJECT
        public:
            AdsRowValidatesDelegates(QObject *parent = 0);
            void paint( QPainter * painter, const QStyleOptionViewItem & option,
                        const QModelIndex & index ) const;
        };

    }
}
#endif // ADSROWVALIDATESDELEGATES_H
