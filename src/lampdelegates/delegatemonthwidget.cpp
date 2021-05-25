#include "delegatemonthwidget.h"
#include "ui_delegatemonthwidget.h"
#include <QDate>

DelegateMonthWidget::DelegateMonthWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DelegateMonthWidget)
{
    ui->setupUi(this);
    listCheckbox[January] = ui->cbJan;
    listCheckbox[February] = ui->cbFeb;
    listCheckbox[March] = ui->cbMar;
    listCheckbox[April] = ui->cbApr;
    listCheckbox[May] = ui->cbMay;
    listCheckbox[June] = ui->cbJun;
    listCheckbox[July] = ui->cbJui;
    listCheckbox[August] = ui->cbAug;
    listCheckbox[September] = ui->cbSen;
    listCheckbox[October] = ui->cbOkt;
    listCheckbox[November] = ui->cbNov;
    listCheckbox[December] = ui->cbDec;
}

DelegateMonthWidget::~DelegateMonthWidget()
{
    delete ui;
}

bool DelegateMonthWidget::setValue(QList<Month> months){
    int ok = 0;
    foreach (Month month, months) {
        if((January <= month) && (month <= December)){
            listCheckbox[month]->setChecked(true);
            ++ok;
        }// else skip
    }
    return (bool)ok;
}
QList<Month> DelegateMonthWidget::getValue(){
    QList<Month> result;
    foreach (QCheckBox* monthB, listCheckbox.values()) {
        if(monthB->isChecked()) {
            QMapIterator<Month, QCheckBox *> i(listCheckbox);
            while(i.findNext(monthB)) {
                 result << i.key();
            }
        }
    }
    return result;
}
