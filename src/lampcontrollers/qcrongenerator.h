/*
    @class Класс позволяет генерировать crontab файл с параметрами для запуска треков
    @autor Кромин О.А
*/
#ifndef QCRONGENERATOR_H
#define QCRONGENERATOR_H
#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QErrorMessage>
#include "trackfullinfo.h"
#include <QTemporaryFile>
#include <memory>
class Controller;

class QCronGenerator: public QObject
{
    Q_OBJECT
private:
    QString mResult;

public:
    explicit QCronGenerator(QObject *parent);
    bool Init(TracksFullInfo tracksFullInfo, QString& lErrorMsg, bool debuglevel);

    /* bool SaveToFile(QString filename  );
    */

    bool Make();
    /*
        Альтернативный вариант для Андрианова, cronPath место куда лягут файлы крона
    */
    bool Make( QString cronPath);

    /*
        Версия для Windows
    */
    bool Make (QString scriptPath, QString mediaPath);

    bool WinMake(QString advFullPath, QString adsContent);
    QString toString();
    std::unique_ptr<Controller> mController;
    QTemporaryFile mTmpFile;

    QString mLastError;
    ~QCronGenerator() override;
signals:

public slots:

};

#endif // QCRONGENERATOR_H
