#pragma once

#ifndef TRIALMESSAGEBOX_H
#define TRIALMESSAGEBOX_H

#include <QMessageBox>
#include <QWidget>

class TrialMessageBox : public QMessageBox
{
public:
    TrialMessageBox( QString text = QString(), QWidget *parent = nullptr);
    ~TrialMessageBox();
};

#endif // TRIALMESSAGEBOX_H
