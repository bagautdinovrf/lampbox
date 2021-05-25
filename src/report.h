#ifndef REPORT_H
#define REPORT_H
#include <QMessageBox>
#include <QWidget>
#include "compositionslist.h"
#include "parser.h"
namespace Ui {
class Report;
}

class Report : public QWidget
{
    Q_OBJECT
    
public:
    explicit Report(QWidget *parent = 0, Qt::WindowFlags f = 0);
    ~Report();
   	CompositionsList *mCompositionsList; 
public slots:
	void Generate();
private:
    Ui::Report *ui;
};

#endif // REPORT_H
