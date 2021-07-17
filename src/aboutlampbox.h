#ifndef ABOUTLAMPBOX_H
#define ABOUTLAMPBOX_H

#include <QDialog>

namespace Ui {
    class AboutLampbox;
}

class AboutLampbox : public QDialog
{
    Q_OBJECT

public:
    explicit AboutLampbox(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags() );
    ~AboutLampbox();

private:
    void mousePressEvent(QMouseEvent * event) override;

private:
    Ui::AboutLampbox *ui;
};

#endif // ABOUTLAMPBOX_H
