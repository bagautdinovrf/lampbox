#pragma once

#ifndef ABOUTMEDIABOXMANAGER_H
#define ABOUTMEDIABOXMANAGER_H

#include <QDialog>

class AboutMediaBoxManager : public QDialog
{
    Q_OBJECT

public:
    explicit AboutMediaBoxManager(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags() );
    ~AboutMediaBoxManager();

signals:
    void doneRequested();

};

#endif // ABOUTMEDIABOXMANAGER_H
