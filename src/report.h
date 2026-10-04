#pragma once

#ifndef REPORT_H
#define REPORT_H
#include <QMessageBox>
#include <QWidget>
#include "compositionslist.h"
class QComboBox;
class QDateEdit;
class QLabel;
class QTableView;
class QStackedWidget;

class Report : public QWidget
{
    Q_OBJECT
    
public:
    explicit Report(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~Report();
    CompositionsList *mCompositionsList = nullptr;
    void setReportDirectory(const QString &directory);
public slots:
	void Generate();
private:
    QComboBox *mMonth = nullptr;
    QDateEdit *mYear = nullptr;
    QTableView *mTable = nullptr;
    QStackedWidget *mResults = nullptr;
    QLabel *mEmptyTitle = nullptr;
    QLabel *mEmptyDescription = nullptr;
    QString mReportDirectory = QStringLiteral("/home/mediabox/mbstatus");
};

#endif // REPORT_H
