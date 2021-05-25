#include "qcrongenerator.h"
//#include "filemaker.h"
#include "controller.h"
#include "cron.h"

QCronGenerator::QCronGenerator(QObject *parent)//:FileMaker( parent )
{
    Q_UNUSED(parent);
}

bool QCronGenerator::Init(TracksFullInfo tracksFullInfo, QString &lErrorMsg, bool dbg){

    mController = new Controller( 0 );

    if (tracksFullInfo.size()==0){
        lErrorMsg += tr("Отсутствует информация о расписании\n");
        mLastError = lErrorMsg;
        return false;
    }
    if (! mController->Init(tracksFullInfo,lErrorMsg,dbg) ){
        lErrorMsg += tr("Не удалось подготовить данные\n");
        mLastError = lErrorMsg;
        return false;
    }
    if( !mController->GenerateCronList(lErrorMsg)){
        lErrorMsg += tr("Ошибка создания списка треков\n");
        mLastError = lErrorMsg;
        return false;
    }
   return true;
}


bool QCronGenerator::Make(){


    CronItemData item;
    mResult.append(START_STRING);
    if (mController->mCronList.size()==0) {
        mLastError = "QCronGenerator::Make mCronList is empty";
        qDebug()<< mLastError;
        return false;
    }

    foreach(item, mController->mCronList)
    {
        mResult.append(QString("%1 %2 %3 %4 %5 root /home/mediabox/scripts/play_advertise.pl %6 %7\n").arg(
                                    item.m, item.h, item.dom, item.mon, item.dow, item.Filename, item.volume));
    }

    if(mTmpFile.open()){
        mLastError = "QCronGenerator::Make TMP FILE ERROR";
        qDebug()<<mLastError;
        QTextStream out(&mTmpFile);
        out << toString();
        return true;
    }
    return false;
}

bool QCronGenerator::WinMake(QString advFullPath, QString adsContent){
    CronItemData item;
    //mResult.append(START_STRING);
    QString play_advertise = QString("perl.exe") + "\"" + advFullPath + "/play_advertise.pl" + "\"";
    QString play_music =QString("perl.exe  ") + "\""+ advFullPath + "/play_music.pl"+"\"";
    mResult.append(QString("* * * * * %1\n").arg(play_music));

    if (mController->mCronList.size()==0) {
        mLastError = "QCronGenerator::Make mCronList is empty";
        qDebug()<< mLastError;
        return false;
    }

    foreach(item, mController->mCronList)
    {
        mResult.append(QString("%1 %2 %3 %4 %5 %6 %7 %8\n").arg(
                                    item.m, item.h, item.dom, item.mon,
                                    item.dow,  play_advertise, item.Filename, item.volume));
    }

    if(mTmpFile.open()){

        QTextStream out(&mTmpFile);
        out << toString();
        return true;
    }
    mLastError = "QCronGenerator::Make TMP FILE ERROR";
    qDebug()<<mLastError;
    return false;
}

bool QCronGenerator::Make(QString cronPath){

    CronItemData item;
    mResult.append(START_STRING);
    if (mController->mCronList.size()==0) {
        qDebug()<<"Make::Empty mCronList";
        return false;
    }

    foreach(item, mController->mCronList)
    {
           mResult.append(QString("%1 %2 %3 %4 %5  c:/myplayer/mpg123.exe c:/lampcontent/ads/%6 \n").arg(
                                    item.m, item.h, item.dom, item.mon, item.dow, item.Filename));
    }


    QString ads = cronPath+"\\ads.tab";
    QFile file2(ads);

    if( !file2.open( QFile::WriteOnly | QFile::Text ) ) {
            qDebug() << "Невозможно открыть файл:" << file2.fileName();
            return false;
    }
    else{

            QTextStream stream(&file2);
            stream<<toString();

            file2.close();

            QFile cronfile(cronPath + "\\cron.tab");
            QFile timetablefile(cronPath + "\\music.tab");

            if( !cronfile.open( QFile::WriteOnly | QFile::Text ) ) {
                qDebug() << "CRON FOR RADIO IS NOT READY";
                qDebug() << "Невозможно открыть файл:" << cronfile.fileName();
                return false;
            }
            else{
                qDebug()<<"CRON FOR RADIO IS READY";

            }
            QTextStream cronstream(&cronfile);
            cronstream<< toString();
            if( !timetablefile.open( QFile::ReadOnly | QFile::Text ) ) {
                qDebug()<<"TIMETABLE NOT OPEN!";
                qDebug() << "Невозможно открыть файл:" << timetablefile.fileName();
            }
            else{


                 while( !timetablefile.atEnd() )
                 {
                   QString text = timetablefile.readLine();



                   cronstream << text;
                 }

            }

             cronfile.close();
        }

    return true;
}

QString QCronGenerator::toString()
{
    return mResult;
}

QCronGenerator::~QCronGenerator(){
    if(mController)
        delete mController;
}
