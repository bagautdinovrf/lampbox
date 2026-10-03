#include "trialmessagebox.h"


TrialMessageBox::TrialMessageBox(QString text, QWidget *parent) :
    QMessageBox(parent)
{
    QString message = "Вы используете пробную версию программы. Приобретите полную версию для использования всех возможностей программы.\n\nАвтор: Руслан Багаутдинов\nПочта: bagautdinovrf@ya.ru";

    if( !text.isEmpty() )
        message = text +"\n\n"+ message;

    QMessageBox::information(parent, "Триальная версия",  message);
}

TrialMessageBox::~TrialMessageBox()
{
    //
}

