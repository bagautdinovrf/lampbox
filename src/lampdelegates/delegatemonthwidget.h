#ifndef DELEGATEMONTHWIDGET_H
#define DELEGATEMONTHWIDGET_H

#include <QWidget>
#include <QCheckBox>
#include <QMap>

#include "types.h"

//#include <QLMCommons>

namespace Ui {
class DelegateMonthWidget;
}
using namespace lampproject;

class DelegateMonthWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit DelegateMonthWidget(QWidget *parent = 0);
    ~DelegateMonthWidget();
    bool setValue(QList<Month> months);
    QList<Month> getValue();
private:
    Ui::DelegateMonthWidget *ui;
    QMap<Month,QCheckBox*> listCheckbox;
};

#endif // DELEGATEMONTHWIDGET_H
