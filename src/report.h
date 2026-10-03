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
    explicit Report(QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~Report();
   	CompositionsList *mCompositionsList; 
public slots:
	void Generate();
private:
    Ui::Report *ui;
};

#endif // REPORT_H
