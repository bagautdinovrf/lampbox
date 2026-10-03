#include "trialmessagebox.h"
#include "restyletheme.h"


TrialMessageBox::TrialMessageBox(QString text, QWidget *parent) :
    QMessageBox(parent)
{
    QString message = "Вы используете пробную версию программы. Приобретите полную версию для использования всех возможностей программы.\n\nАвтор: Руслан Багаутдинов\nПочта: bagautdinovrf@ya.ru";

    if( !text.isEmpty() )
        message = text +"\n\n"+ message;

    setWindowTitle(tr("Пробная версия MediaBoxManager"));
    setText(message);
    setTextFormat(Qt::PlainText);
    setIcon(QMessageBox::Information);
    setStandardButtons(QMessageBox::Ok);
    setFont(Restyle::font());
    Restyle::surface(this, QStringLiteral("dialog"));
    exec();
}

TrialMessageBox::~TrialMessageBox()
{
    //
}
