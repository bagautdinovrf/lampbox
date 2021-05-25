#ifndef _BOX_LOG_H
#define _BOX_LOG_H

#include <QFile>

class BoxLog
{
public:
    BoxLog();
    explicit BoxLog( const QString &text);
    explicit BoxLog( int num);
    explicit BoxLog( uint num );

    BoxLog &operator <<(QString text);
    BoxLog &operator <<(int num);

    ~BoxLog();

private:
    void writeLog( const QString &text ) const;
    void writeLog( int num ) const;
    void writeLog( uint num ) const;

    void init();


private:
    QFile                                  * mLogFile;
    QString                                 mText;
    bool                                    mStream;

};

#endif // _BOX_LOG_H
