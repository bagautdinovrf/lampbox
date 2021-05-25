#include <QApplication>
#include <QTextCodec>
#include <QSharedMemory>
#include <QDebug>
#include <QMessageBox>
#include <QStyleFactory>
#include <QProcess>
#include <QObject>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

#include <iostream>
#include "mainwindow.h"
#include "stationmanager.h"
#include "trialmessagebox.h"


bool checkEnvironment(){
    if( !StationManager::Instance().update() ){
    #ifdef WIN32
            if( QMessageBox::question(NULL, QObject::tr("LampBox"),
                                         QObject::tr("%1 Хотите скачать и установить?").arg(StationManager::Instance().lastError())) == QMessageBox::Yes)
            {
                qWarning() << QApplication::applicationDirPath()+"/getstation.exe";
                QDir dir(QApplication::applicationDirPath());
                const QString url= QString("file:///%1")
                        .arg(dir.absoluteFilePath("getstation.exe"));
                if( QDesktopServices::openUrl(QUrl(url,QUrl::TolerantMode))){
                    QMessageBox::information(NULL, QObject::tr("Установите LampPlayer."),
                                                   QObject::tr(
                                                      "Следуйте несложным инструкциям по установке <b>LampPlayer</b>.<br>"
                                                      "После завершения установки, вы сможете воспользоваться всеми возможностями <b>LampBox</b>.<br>"
                                                      "По всем возникшим вопросам связывайтесь с нами:<br>"
                                                      "По телефону: <b>8(495)729-73-48</b><br>"
                                                      "По почте: <a href=\"mailto:support@lampmedia.ru\">support@lampmedia.ru</a><br>"
                                                      "<br>"
                                                      "Пока вы читаете это сообщение, мы загружаем установщик <b>LampPlayer</b>.<br>"
                                                      "Если на этом компьютере нет интернета - <a href=\"http://client.lampmusic.ru/dl/lampstation.exe\">скачайте и установите LampPlayer</a> вручную."));
                                                    return false;
                }

                return checkEnvironment();
            } else {
                return false;
            }
    #endif
    }
    return true;
}

bool checkLampPlayer()
{
    if( !StationManager::Instance().update() ) {
        QMessageBox::warning(nullptr, "LampBox", "Внимание! У вас не установлен <b>LampPlayer</b>!<br>"
                                                 "Для правильной работы <b>LampBox</b> скачайте<br>"
                                                 "и установите последнюю версию <b>LampPlayer</b>.<br><br>"
                                                 "По всем возникшим вопросам связывайтесь с нами:<br>"
                                                 "По телефону: <b>8(495)729-73-48</b><br>"
                                                 "По почте: <a href=\"mailto:support@lampmedia.ru\">support@lampmedia.ru</a>"
                             );
        return false;
    }
return true;
}

int main(int argc, char *argv[])
{
    if(argc == 2){
        if(QString(argv[1]) == "version") {
           std::cout << VERSION;
           QFile file("version");
           if(file.open(QIODevice::WriteOnly)) {
            file.write( VERSION );
            file.close();
           }
           return 0;
        }
    }

    QApplication a(argc, argv);

    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForLocale(codec);

    QSharedMemory mem("LampBoxMemory");
    if( mem.attach() ) {
        QMessageBox msgBox;
        msgBox.setText("Программа уже запущена!");
        msgBox.exec();
        return 1;
    } else {
        mem.create(1);
    }
#if QT_VERSION >= 0x050100
    #ifdef __linux__
        QApplication::setStyle(QStyleFactory::create("Gtk"));
    #else
        QApplication::setStyle(QStyleFactory::create("Fusion"));
    #endif
#endif

    if( !checkLampPlayer() )
        return 1;

    if( StationManager::Instance().trial() )
        TrialMessageBox();

    MainWindow w;
    w.showMaximized();
    return a.exec();
}
