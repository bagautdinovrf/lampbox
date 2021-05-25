#ifndef TRACKDELEGATE_H
#define TRACKDELEGATE_H
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
#include "noteditabledelegate.h"
//#include <QLMCommons>
//#include <QLMObjects>
//#include <QLMTools>
//#include <QLMModels>

//using namespace lampproject::rtools;
//using namespace lampproject::lmobject;
//using namespace lampproject::models;

class  TrackDelegate : public QItemDelegate
{
        Q_OBJECT
        public:
        TrackDelegate(int frequencyColumn, QObject *parent=0);

           // void setEditorData(QWidget *editor, const QModelIndex &index) const;
            void updateEditorGeometry(QWidget *editor,
                         const QStyleOptionViewItem &option, const QModelIndex &/* index */) const;
           // void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;

            QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const;

        private :
            int mFrequencyColumn;
};
#endif // TRACKDELEGATE_H
