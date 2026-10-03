#include "aboutlampbox.h"
#include "ui_aboutlampbox.h"

namespace {
constexpr int buildYear = (__DATE__[7] - '0') * 1000
        + (__DATE__[8] - '0') * 100
        + (__DATE__[9] - '0') * 10
        + (__DATE__[10] - '0');
}

AboutLampbox::AboutLampbox(QWidget *parent, Qt::WindowFlags f) :
    QDialog(parent, f),
    ui(new Ui::AboutLampbox)
{
    ui->setupUi(this);
    ui->label_version->setText("Версия: " + QString(VERSION) );
    ui->label_years->setText(QStringLiteral("2012-%1").arg(buildYear));
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
