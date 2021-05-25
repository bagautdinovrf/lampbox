#include "parser.h"

Parser::Parser(QObject *parent) :
    QObject(parent)
{
}


bool Parser::Init(QString str){
    mCsvFile = str;
    mFile.setFileName(str);
    if ( QFile::exists(str)) {

        return true;}
    return false;
}


bool Parser::Parse(){

    if(mFile.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&mFile);
        QString str;
        while (!stream.atEnd())
        {
            str = stream.readLine();
            QStringList ln = str.split(";");
            mList.append(ln);
            qDebug() << str;
        }
        if(stream.status()!= QTextStream::Ok)
        {
            qDebug() << "Ошибка чтения файла";
        }
        mFile.close();
    }
return true;
}
void Parser::Show(){
    for(int i =0;i<mList.size();i++){
        for( int j=0;j<mList[i].size();j++){
            qDebug()<<mList[i][j];
        }
    };

}
