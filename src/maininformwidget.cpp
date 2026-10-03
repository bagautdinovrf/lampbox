
#include "maininformwidget.h"
#include "ui_maininformwidget.h"
#include "restyletheme.h"


#include <QTimer>


MainInformWidget::MainInformWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainInformWidget)
{
    ui->setupUi(this);
    Restyle::surface(ui->widgetInfo, QStringLiteral("selection"));
    ui->labelInfo->setFont(Restyle::font(12));
    ui->labelInfo->setWordWrap(true);
    ui->labelInfo->setTextFormat(Qt::PlainText);
    ui->gridLayout_5->setContentsMargins(12, 4, 7, 4);
    ui->pushButtonHideInfo->setIcon(Restyle::icon(QStringLiteral("close")));
    ui->pushButtonHideInfo->setAccessibleName(tr("Закрыть сообщение"));
    ui->pushButtonHideInfo->setToolTip(tr("Закрыть сообщение"));
    ui->pushButtonHideInfo->setFixedSize(29, 29);
    Restyle::button(ui->pushButtonHideInfo, QStringLiteral("icon"));
    mHideWidgetTimer.setSingleShot(true);
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
    mHideWidgetTimer.stop();
    this->hide();
}
