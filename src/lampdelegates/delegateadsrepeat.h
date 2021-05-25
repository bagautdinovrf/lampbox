#ifndef DELEGATEADSREPEAT_H
#define DELEGATEADSREPEAT_H

#include <QWidget>
#include <QStyledItemDelegate>
#include "types.h"
//#include <QLMWidgets>
//#include <QLMCommons>

namespace Ui {
class DelegateAdsRepeat;
}


namespace lampproject{
    namespace delegate {
        class DelegateAdsRepeatEdit : public QStyledItemDelegate
        {
            Q_OBJECT
        public:
            DelegateAdsRepeatEdit(QObject *parent = 0);
            QString displayText( const QVariant & value, const QLocale & locale ) const;
            QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const;

            void setEditorData(QWidget *editor, const QModelIndex &index) const;
            void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const;

            void updateEditorGeometry(QWidget *editor,
                const QStyleOptionViewItem &option, const QModelIndex &index) const;
            QList<lampproject::DayOfWeek> getDays(QString value) const;
        };
        class DelegateAdsRepeat : public QWidget
        {
            Q_OBJECT
            QString currentValue;
        public:
            explicit DelegateAdsRepeat(QWidget *parent = 0);
            ~DelegateAdsRepeat();
            QString getValue();
            bool setValue(QString value);
        private slots:
            void on_horizontalSlider_valueChanged(int value);

            void on_timeEdit_timeChanged(const QTime &time);

        private:
            Ui::DelegateAdsRepeat *ui;
        };
    }
}
#endif // DELEGATEADSREPEAT_H
