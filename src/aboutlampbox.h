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
    explicit AboutLampbox(QWidget *parent = 0, Qt::WindowFlags f = 0);
    ~AboutLampbox();

private:
    void mousePressEvent(QMouseEvent * event) override;

private:
    Ui::AboutLampbox *ui;
};

#endif // ABOUTLAMPBOX_H
