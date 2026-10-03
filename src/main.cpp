#include <QApplication>
#include <QSharedMemory>
#include <QMessageBox>
#include <QStyleFactory>
#include <QIcon>
#include <QDebug>

#include <exception>
#include <iostream>
#include "mainwindow.h"
#include "stationmanager.h"
#include "trialmessagebox.h"
#include "settings.h"
#include "restyletheme.h"


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
    QCoreApplication::setApplicationName(QStringLiteral("MediaBoxManager"));
    QCoreApplication::setApplicationVersion(QStringLiteral(VERSION));
    QApplication::setApplicationDisplayName(QStringLiteral("MediaBoxManager"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/player/icons/app.ico")));

    try {
        Settings settings;
        Restyle::install(a, settings.appearanceId(), settings.themeId());
    } catch (const std::exception &error) {
        qCritical().noquote() << error.what();
        return 1;
    }

    // Keep the legacy key to prevent concurrent access by an older LampBox.
    QSharedMemory mem("LampBoxMemory");
    if( mem.attach() ) {
        QMessageBox msgBox;
        msgBox.setText("Программа уже запущена!");
        msgBox.exec();
        return 1;
    } else {
        mem.create(1);
    }

    auto &station = StationManager::Instance();
    if (!station.lastError().isEmpty()) {
        qCritical().noquote() << station.lastError();
        return 1;
    }
    if( station.trial() )
        TrialMessageBox();

    MainWindow w;
    w.showMaximized();
    return a.exec();
}
