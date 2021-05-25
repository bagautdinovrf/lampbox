INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

DEFINES += LMCONTROLLERS

SOURCES+=   $$PWD/balance.cpp \
            $$PWD/qcrongenerator.cpp \
            $$PWD/controller.cpp \
            $$PWD/hours.cpp \
            $$PWD/xmlparse.cpp \
            $$PWD/trackfullinfo.cpp \
            $$PWD/advertjsongenerator.cpp \
            $$PWD/advertcsvgenerator.cpp \
            $$PWD/trackdistributor.cpp

HEADERS+=   $$PWD/balance.h \
            $$PWD/qcrongenerator.h \
            $$PWD/controller.h \
            $$PWD/hours.h \
            $$PWD/cron.h  \
            $$PWD/xmlparse.h \
            $$PWD/trackfullinfo.h \
            $$PWD/advertjsongenerator.h \
            $$PWD/advertcsvgenerator.h \
            $$PWD/trackdistributor.h
