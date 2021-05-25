QT  += core gui xml
QT  += widgets

CONFIG  += c++11

TARGET = lampbox
TEMPLATE = app
VERSION = 1.0.5.4
DEFINES += VERSION=\\\"$$VERSION\\\"

debug {
  DEFINES += SHOW_DEBUG_OUTPUT
}

SOURCES += main.cpp\
    mainwindow.cpp \
    channeldata.cpp \
    mediadata.cpp \
    channelmanager.cpp \
    channelmodel.cpp \
    mediamanager.cpp \
    mediamodel.cpp \
    timedelegate.cpp \
    mediacontroller.cpp \
    composition.cpp \
    compositionslist.cpp \
    parser.cpp \
    report.cpp \
    settingsdialog.cpp \
    settings.cpp \
    volumedelegate.cpp \
    advertmanager.cpp \
    advertdata.cpp \
    advertmodel.cpp \
    dateeditdelegate.cpp \
    simplelineeditdelegate.cpp \
    stationmanager.cpp \
    boxlog.cpp \
    aboutlampbox.cpp \
    informer.cpp \
    maininformwidget.cpp \
    trialmessagebox.cpp

HEADERS  += mainwindow.h \
    channeldata.h \
    mediadata.h \
    channelmanager.h \
    channelmodel.h \
    mediamanager.h \
    lampdata.h \
    mediamodel.h \
    timedelegate.h \
    mediacontroller.h \
    composition.h \
    compositionslist.h \
    parser.h \
    report.h \
    settingsdialog.h \
    settings.h \
    volumedelegate.h \
    advertmanager.h \
    advertdata.h \
    advertmodel.h \
    dateeditdelegate.h \
    simplelineeditdelegate.h \
    stationmanager.h \
    boxlog.h \
    aboutlampbox.h \
    informer.h \
    maininformwidget.h \
    trialmessagebox.h

FORMS    += mainwindow.ui \
    report.ui \
    settingsdialog.ui \
    aboutlampbox.ui \
    maininformwidget.ui

RESOURCES += icons.qrc

include(lampdelegates/delegates.pri)
include(lampcontrollers/controllers.pri)
include(taglib/taglib-master/taglib/taglib.pri)

OTHER_FILES += app.rc
RC_FILE      = app.rc
