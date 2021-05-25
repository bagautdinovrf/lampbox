INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
!contains(DEFINES,LMDELEGATES){
DEFINES += LMDELEGATES
}
include($$PWD/widgets/exwidgets.pri)

SOURCES+=   $$PWD/delegatevolumeedit.cpp \
            $$PWD/delegatemonthedit.cpp \
            $$PWD/delegatemonthwidget.cpp \
            $$PWD/datedelegate.cpp \
            $$PWD/lineeditdelegate.cpp \
            $$PWD/frequencydelegate.cpp \
            $$PWD/trackdelegate.cpp \
            $$PWD/delegatedaysoftheweek.cpp \
            $$PWD/delegateadsrepeat.cpp \
            $$PWD/delegatedatetimeperiod.cpp \
            $$PWD/onairdelegate.cpp


HEADERS+=   $$PWD/delegatevolumeedit.h \
            $$PWD/delegatemonthedit.h \
            $$PWD/delegatemonthwidget.h \
            $$PWD/datedelegate.h \
            $$PWD/lineeditdelegate.h \
            $$PWD/frequencydelegate.h \
            $$PWD/trackdelegate.h \
            $$PWD/delegatedaysoftheweek.h \
            $$PWD/delegateadsrepeat.h \
            $$PWD/delegatedatetimeperiod.h \
            $$PWD/types.h \
            $$PWD/onairdelegate.h \
            $$PWD/noteditabledelegate.h

contains(DEFINES,LMOBJECT){
SOURCES +=  $$PWD/comboboxdelegate.cpp \
            $$PWD/treeitemdelegate.cpp \
            $$PWD/radiobuttondelegate.cpp

HEADERS +=  $$PWD/comboboxdelegate.h \
            $$PWD/treeitemdelegate.h \
            $$PWD/radiobuttondelegate.h
}

FORMS +=    $$PWD/delegatedaysoftheweek.ui \
            $$PWD/TimePeriodEditDelegate.ui \
            $$PWD/delegatemonthwidget.ui \
            $$PWD/delegateadsrepeat.ui \
            $$PWD/delegatedatetimeperiod.ui

RESOURCES += \
    $$PWD/delegateres.qrc
