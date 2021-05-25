#ifndef PARSER_H
#define PARSER_H
#include <QFile>
#include <QObject>
#include <QTextStream>
#include <QStringList>
#include <QDebug>
class Parser : public QObject
{
    Q_OBJECT
public:
    explicit Parser(QObject *parent = 0);
    bool Init(QString filename);
    bool Parse();
    QString mCsvFile;
    QList<QStringList> mList;
    QFile mFile;
    void Show();
signals:
    
public slots:
    
};

#endif // PARSER_H
