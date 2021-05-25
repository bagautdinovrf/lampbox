#include "report.h"
#include "ui_report.h"

Report::Report(QWidget *parent, Qt::WindowFlags f) :
    QWidget(parent, f),
    ui(new Ui::Report)
{

    ui->setupUi(this);
    connect(ui->pbGenerate,SIGNAL(clicked()),SLOT(Generate()));
    QStringList month;

    for (int month = 1; month <= 12; ++month)
            ui->cbMonth->addItem(QDate::longMonthName(month));
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
    QString strMonth;
    strMonth.sprintf("%02d", month);


    QString filePath = QString("/home/mediabox/mbstatus/") + strMonth+strYear + ".csv";
     qDebug()<<" file: "<<filePath;
    if( mCompositionsList->Init(filePath)) {
         ui->tableView->setModel(mCompositionsList);
    }
    else{

        QMessageBox::information(this, trUtf8("Файл отчетов не найден"), trUtf8("За этот период отчетов нет"));

    }

}
