
#include "maininformwidget.h"
#include "ui_maininformwidget.h"


#include <QTimer>


MainInformWidget::MainInformWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainInformWidget)
{
    ui->setupUi(this);
    connect(&mHideWidgetTimer, SIGNAL(timeout()), SLOT(hideInform()) );
}

MainInformWidget::~MainInformWidget()
{
    delete ui;
}

void MainInformWidget::setMessage(const QString &text)
{
    ui->labelInfo->setText( text );
    mTextMessage = text;
}

void MainInformWidget::inform(const QString &text)
{
    setMessage(text);
    this->show();
    mHideWidgetTimer.start( 10000 );
//    mHideWidgetTimer.singleShot(10000, this, SLOT( hideInform() ) );
//    QTimer::singleShot(10000, this, SLOT( hideInform() ) );
}

void MainInformWidget::hideInform()
{
    this->hide();
}
