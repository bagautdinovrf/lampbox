#include "aboutlampbox.h"
#include "ui_aboutlampbox.h"

AboutLampbox::AboutLampbox(QWidget *parent, Qt::WindowFlags f) :
    QDialog(parent, f),
    ui(new Ui::AboutLampbox)
{
    ui->setupUi(this);
    ui->label_version->setText("Версия: " + QString(VERSION) );
}

AboutLampbox::~AboutLampbox()
{
    delete ui;
}

void AboutLampbox::mousePressEvent(QMouseEvent *event)
{
    QDialog::mousePressEvent(event);
    close();
}
