#include "trialmessagebox.h"


TrialMessageBox::TrialMessageBox(QString text, QWidget *parent) :
    QMessageBox(parent)
{
    QString message = "Вы используете триальную версию программу, приобретите полную версию для использования всех возможностей программы.\n\n Наш сайт: http://www.lampmedia.ru \n Телефон: (495)729-73-48\n E-Mail: office@lampmedia.ru";

    if( !text.isEmpty() )
        message = text +"\n\n"+ message;

    QMessageBox::information(parent, "Триальная версия",  message);
}

TrialMessageBox::~TrialMessageBox()
{
    //
}

