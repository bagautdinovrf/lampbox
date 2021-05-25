#include "boxlog.h"


#include <QTextStream>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QTextCodec>

BoxLog::BoxLog() :
    mLogFile( new QFile() ),
    mStream(true)
{
    init();
}

BoxLog::BoxLog(const QString &text) :
    mLogFile( new QFile() ),
    mStream(false)
{
    init();
    writeLog(text);
}


BoxLog::BoxLog(int num) :
    mLogFile( new QFile() ),
    mStream(false)
{
    init();
    writeLog(num);
}


BoxLog::BoxLog(uint num ) :
    mLogFile( new QFile() ),
    mStream(false)
{
    init();
    writeLog(num);
}

BoxLog::~BoxLog()
{
    if(mStream) {
        writeLog(mText);
    }

    if(mLogFile) {
        if( mLogFile->isOpen() )
            mLogFile->close();
        delete mLogFile;
    }
}


void BoxLog::init()
{
    if( mLogFile )
      mLogFile->setFileName( qApp->applicationDirPath() +QDir::separator()+ "lampbox.log");
}


void BoxLog::writeLog( const QString &text ) const
{
    if( !mLogFile )
        return;
    if(!mLogFile->open(QFile::Append | QFile::Text))
        return;

    QTextStream stream(mLogFile);
    stream << QDateTime::currentDateTime().toString("[dd.MM.yyyy_hh:mm:ss.zzz] ")
           << text << '\n';
    mLogFile->close();

#ifdef SEND_TO_DEBUG_LOG_MESSAGE
    qDebug() << text;
#endif
}


void BoxLog::writeLog(int num) const
{
    writeLog( QString::number(num) );
}


void BoxLog::writeLog(uint num) const
{
    writeLog( QString::number(num));
}


BoxLog &BoxLog::operator <<(QString text)
{
    if(!mText.isEmpty())
        mText.append(' ');
    mText.append( text );
    return *this;
}

BoxLog &BoxLog::operator <<(int num)
{
    return operator <<(QString::number(num));
}
