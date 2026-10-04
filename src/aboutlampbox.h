#pragma once

#ifndef ABOUTLAMPBOX_H
#define ABOUTLAMPBOX_H

#include <QDialog>

class AboutLampbox : public QDialog
{
    Q_OBJECT

public:
    explicit AboutLampbox(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags() );
    ~AboutLampbox();

signals:
    void doneRequested();

};

#endif // ABOUTLAMPBOX_H
