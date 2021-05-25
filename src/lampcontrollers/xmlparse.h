/**
    @file    Класс получает данные из XML файла
    @author  Климов А.В.
*/
#ifndef XMLParse_H
#define XMLParse_H
#include <QListView>
#include <QIODevice>
#include <QObject>
#include <QtXml/QtXml>
#include <QtXml/QDomElement>
#include <QListView>
#include <qlistview.h>
#include <iostream>
#include <QMap>
//#include <QLMCommons>

using namespace std;

class XMLParse : public QObject
{
    Q_OBJECT
public:
    explicit XMLParse(QObject *parent = 0);
    QDomDocument doc;

    //! @fn Init() - Функция принимает имя файла для парсинга
    //! @ret true - удача |false - не удалось
    bool Init(const QString & XmlFilename);

    //! @fn Proceed()
    bool Proceed(QString wwf, QString& PathValue);

    bool Proceed(QString wwf, QString tagName ,QString& value );

    QFile file;

    QString mErrorMsg;

    ~XMLParse();
private:
  void parseEntry(const QDomElement &element
                  );
signals:

public slots:

};

#endif // XMLParse_H
