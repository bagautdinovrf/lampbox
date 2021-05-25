#include "xmlparse.h"

XMLParse::XMLParse(QObject *parent) :
    QObject(parent)
{


}
bool XMLParse::Init(const QString & filename){
    //QDomDocument doc("mydocument");
    file.setFileName( filename );
    if (!file.open(QIODevice::ReadOnly)){
        mErrorMsg = "XMLParse::Init> File wasn't opened \n";
        return false;
    }

    if (!doc.setContent(&file)) {
        mErrorMsg = "XMLParse::ctor> File wasn't parsed XML VALIDATION FAILED\n";
        file.close();
        return false;
    }
    return true;
}

bool XMLParse::Proceed(QString wwf, QString& value ){
    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
    while(!n.isNull()) {
        QDomElement e = n.toElement();
        if(!e.isNull()) {
            if(e.tagName() == wwf){
                QDomNode sub = e.firstChild();
                while(!sub.isNull()){
                    QDomElement sube = sub.toElement();
                    if(!sube.isNull()){
                        if (sube.tagName()==QString("path") ){
                            value=sube.text();
                            //Нашли
                            return true;
                        }
                    }
                    sub = sub.nextSibling();
                }
            }
        }
        n = n.nextSibling();
    }
    return false;
}

bool XMLParse::Proceed(QString wwf, QString tagName, QString& value ){
    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
    while(!n.isNull()) {
        QDomElement e = n.toElement();
        if(!e.isNull()) {
            if(e.tagName() == wwf){
                QDomNode sub = e.firstChild();
                while(!sub.isNull()){
                    QDomElement sube = sub.toElement();
                    if(!sube.isNull()){
                        if (sube.tagName()==tagName ){
                            value=sube.text();
                            //Нашли
                            return true;
                        }
                    }
                    sub = sub.nextSibling();
                }
            }
        }
        n = n.nextSibling();
    }
    return false;
}
void XMLParse::parseEntry(const QDomElement &element){
Q_UNUSED(element)
}

XMLParse::~XMLParse(){
    if (file.isOpen()) file.close();
}
