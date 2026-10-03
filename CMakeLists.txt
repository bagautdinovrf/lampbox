cmake_minimum_required(VERSION 3.28)

project(MediaBoxManager VERSION 1.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# The project uses headers, not C++ modules; skip compiler dependency scans.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Widgets Xml)
qt_standard_project_setup(REQUIRES 6.12)

include(CTest)
include(GNUInstallDirs)
include(cmake/TagLib.cmake)

# An object library keeps resources linked into both the application and tests.
add_library(MediaBoxManagerCore OBJECT
    src/aboutlampbox.cpp
    src/aboutlampbox.h
    src/advertdata.cpp
    src/advertdata.h
    src/advertmanager.cpp
    src/advertmanager.h
    src/advertmodel.cpp
    src/advertmodel.h
    src/boxlog.cpp
    src/boxlog.h
    src/channeldata.cpp
    src/channeldata.h
    src/channelmanager.cpp
    src/channelmanager.h
    src/channelmodel.cpp
    src/channelmodel.h
    src/composition.cpp
    src/composition.h
    src/compositionslist.cpp
    src/compositionslist.h
    src/dateeditdelegate.cpp
    src/dateeditdelegate.h
    src/informer.cpp
    src/informer.h
    src/lampdata.h
    src/maininformwidget.cpp
    src/maininformwidget.h
    src/mainwindow.cpp
    src/mainwindow.h
    src/mediacontroller.cpp
    src/mediacontroller.h
    src/mediadata.cpp
    src/mediadata.h
    src/mediamanager.cpp
    src/mediamanager.h
    src/mediamodel.cpp
    src/mediamodel.h
    src/parser.cpp
    src/parser.h
    src/report.cpp
    src/report.h
    src/settings.cpp
    src/settings.h
    src/settingsdialog.cpp
    src/settingsdialog.h
    src/simplelineeditdelegate.cpp
    src/simplelineeditdelegate.h
    src/stationmanager.cpp
    src/stationmanager.h
    src/timedelegate.cpp
    src/timedelegate.h
    src/trialmessagebox.cpp
    src/trialmessagebox.h
    src/volumedelegate.cpp
    src/volumedelegate.h
    src/lampcontrollers/advertcsvgenerator.cpp
    src/lampcontrollers/advertcsvgenerator.h
    src/lampcontrollers/advertjsongenerator.cpp
    src/lampcontrollers/advertjsongenerator.h
    src/lampcontrollers/balance.cpp
    src/lampcontrollers/balance.h
    src/lampcontrollers/controller.cpp
    src/lampcontrollers/controller.h
    src/lampcontrollers/cron.h
    src/lampcontrollers/hours.cpp
    src/lampcontrollers/hours.h
    src/lampcontrollers/qcrongenerator.cpp
    src/lampcontrollers/qcrongenerator.h
    src/lampcontrollers/trackdistributor.cpp
    src/lampcontrollers/trackdistributor.h
    src/lampcontrollers/trackfullinfo.cpp
    src/lampcontrollers/trackfullinfo.h
    src/lampcontrollers/xmlparse.cpp
    src/lampcontrollers/xmlparse.h
    src/lampdelegates/datedelegate.cpp
    src/lampdelegates/datedelegate.h
    src/lampdelegates/delegateadsrepeat.cpp
    src/lampdelegates/delegateadsrepeat.h
    src/lampdelegates/delegatedatetimeperiod.cpp
    src/lampdelegates/delegatedatetimeperiod.h
    src/lampdelegates/delegatedaysoftheweek.cpp
    src/lampdelegates/delegatedaysoftheweek.h
    src/lampdelegates/delegatemonthedit.cpp
    src/lampdelegates/delegatemonthedit.h
    src/lampdelegates/delegatemonthwidget.cpp
    src/lampdelegates/delegatemonthwidget.h
    src/lampdelegates/delegatevolumeedit.cpp
    src/lampdelegates/delegatevolumeedit.h
    src/lampdelegates/frequencydelegate.cpp
    src/lampdelegates/frequencydelegate.h
    src/lampdelegates/lineeditdelegate.cpp
    src/lampdelegates/lineeditdelegate.h
    src/lampdelegates/noteditabledelegate.h
    src/lampdelegates/onairdelegate.cpp
    src/lampdelegates/onairdelegate.h
    src/lampdelegates/trackdelegate.cpp
    src/lampdelegates/trackdelegate.h
    src/lampdelegates/types.h
    src/lampdelegates/widgets/color.cpp
    src/lampdelegates/widgets/color.hpp
    src/lampdelegates/widgets/drawing.cpp
    src/lampdelegates/widgets/drawing.hpp
    src/lampdelegates/widgets/sliderwidget.cpp
    src/lampdelegates/widgets/sliderwidget.h
    src/lampdelegates/widgets/switch.cpp
    src/lampdelegates/widgets/switch.hpp
    src/aboutlampbox.ui
    src/maininformwidget.ui
    src/mainwindow.ui
    src/report.ui
    src/settingsdialog.ui
    src/lampdelegates/TimePeriodEditDelegate.ui
    src/lampdelegates/delegateadsrepeat.ui
    src/lampdelegates/delegatedatetimeperiod.ui
    src/lampdelegates/delegatedaysoftheweek.ui
    src/lampdelegates/delegatemonthwidget.ui
    src/icons.qrc
    src/lampdelegates/delegateres.qrc
    src/lampdelegates/widgets/volumeslider.qrc
)
set_target_properties(MediaBoxManagerCore PROPERTIES AUTORCC ON)
target_compile_features(MediaBoxManagerCore PUBLIC cxx_std_23)
target_include_directories(MediaBoxManagerCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lampcontrollers"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lampdelegates"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lampdelegates/widgets"
)
target_compile_definitions(MediaBoxManagerCore PUBLIC
    VERSION="${PROJECT_VERSION}"
    LMCONTROLLERS LMDELEGATES LMEXWIDGETS
    "$<$<CONFIG:Debug>:SHOW_DEBUG_OUTPUT>"
)
target_link_libraries(MediaBoxManagerCore PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml LampBox::TagLib
)
if(MSVC)
    target_compile_options(MediaBoxManagerCore PUBLIC /utf-8 /permissive- /Zc:__cplusplus)
    target_compile_options(MediaBoxManagerCore PRIVATE /W4)
else()
    target_compile_options(MediaBoxManagerCore PRIVATE -Wall -Wextra)
endif()

qt_add_executable(MediaBoxManager WIN32 MACOSX_BUNDLE src/main.cpp)
target_link_libraries(MediaBoxManager PRIVATE MediaBoxManagerCore)
if(WIN32)
    target_sources(MediaBoxManager PRIVATE src/app.rc)
endif()

install(TARGETS MediaBoxManager
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
qt_generate_deploy_app_script(
    TARGET MediaBoxManager
    OUTPUT_SCRIPT mediaboxmanager_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT "${mediaboxmanager_deploy_script}")

add_subdirectory(MediaBoxPlayer)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
