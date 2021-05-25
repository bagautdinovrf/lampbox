#ifndef DELEGATEDAYSOFTHEWEEK_H
#define DELEGATEDAYSOFTHEWEEK_H

#include <QWidget>
#include <QStyledItemDelegate>
#include <QPushButton>
#include "types.h"
//#include <QLMWidgets>
//#include <QLMCommons>

namespace Ui {
    class DelegateDaysOfTheWeek;
}

//using namespace lampproject;

namespace lampproject{
    namespace delegate {
        class DelegateDayOfWeekEdit : public QStyledItemDelegate
        {
            Q_OBJECT
        public:
            DelegateDayOfWeekEdit(QObject *parent = 0);
            QString displayText( const QVariant & value, const QLocale & locale ) const;
            QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const;

            void setEditorData(QWidget *editor, const QModelIndex &index) const;
            void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const;

            void updateEditorGeometry(QWidget *editor,
                const QStyleOptionViewItem &option, const QModelIndex &index) const;
            QList<lampproject::DayOfWeek> getDays(QString value) const;
        signals:
            void setDataMultiple(QString value) const;
        };

        class DelegateDaysOfTheWeek : public QWidget
        {
            Q_OBJECT

        public:
            explicit DelegateDaysOfTheWeek(QWidget *parent = 0);
            ~DelegateDaysOfTheWeek();
            bool setValue(QList<DayOfWeek> dow);
            QList<DayOfWeek> getValue();
        private:
            Ui::DelegateDaysOfTheWeek *ui;
            QMap<lampproject::DayOfWeek,QPushButton*> listButtons;
        };
    }
}


#endif // DELEGATEDAYSOFTHEWEEK_H
