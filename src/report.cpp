#include "report.h"
#include "ui_report.h"
#include <QLocale>

Report::Report(QWidget *parent, Qt::WindowFlags f) :
    QWidget(parent, f),
    ui(new Ui::Report)
{

    ui->setupUi(this);
    connect(ui->pbGenerate,SIGNAL(clicked()),SLOT(Generate()));
    for (int month = 1; month <= 12; ++month)
        ui->cbMonth->addItem(QLocale::system().monthName(month, QLocale::LongFormat));
    ui->yearEdit->setDisplayFormat("yyyy");
}

Report::~Report()
{
    delete ui;
}

void Report::Generate(){
    mCompositionsList = new CompositionsList(this);
    QDate startDate = ui->yearEdit->date();

    QString strYear = startDate.toString("yy");
    int month = ui->cbMonth->currentIndex()+1;
    const QString strMonth = QString::number(month).rightJustified(2, '0');


    QString filePath = QString("/home/mediabox/mbstatus/") + strMonth+strYear + ".csv";
     qDebug()<<" file: "<<filePath;
    if( mCompositionsList->Init(filePath)) {
         ui->tableView->setModel(mCompositionsList);
    }
    else{

        QMessageBox::information(this, tr("Файл отчетов не найден"), tr("За этот период отчетов нет"));

    }

}
