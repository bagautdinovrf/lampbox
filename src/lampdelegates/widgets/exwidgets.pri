INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
!contains(DEFINES,LMEXWIDGETS){
    DEFINES += LMEXWIDGETS
}

SOURCES  +=   $$PWD/color.cpp \
            $$PWD/drawing.cpp \
            $$PWD/switch.cpp \
            $$PWD/sliderwidget.cpp

HEADERS  +=   $$PWD/color.hpp \
                $$PWD/drawing.hpp \
                $$PWD/switch.hpp \
                $$PWD/sliderwidget.h

RESOURCES += \
            $$PWD/volumeslider.qrc
