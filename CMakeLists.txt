cmake_minimum_required(VERSION 3.28)

project(MediaBoxManager VERSION 1.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# The project uses headers, not C++ modules; skip compiler dependency scans.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Widgets Network)
qt_standard_project_setup(REQUIRES 6.12)

include(GNUInstallDirs)
include(cmake/TagLib.cmake)
add_subdirectory(common)

# Domain, persistence and application services build without Qt Widgets.
function(mediabox_manager_library target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(${target} PUBLIC /utf-8 /permissive- /Zc:__cplusplus)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()
add_library(MediaBoxScheduleCore STATIC
    src/schedulecore/schedulecore.cpp
    src/schedulecore/schedulecore.h
)
mediabox_manager_library(MediaBoxScheduleCore)

add_library(MediaBoxScheduleStorage STATIC
    src/projectrepository.cpp
    src/scheduleimport.cpp
    src/projectrepository.h
    src/scheduleimport.h
)
mediabox_manager_library(MediaBoxScheduleStorage)

add_library(MediaBoxMediaLibrary STATIC
    src/mediaimportservice.cpp
    src/mediaimportservice.h
    src/mediadata.cpp
    src/mediadata.h
)
mediabox_manager_library(MediaBoxMediaLibrary)

add_library(MediaBoxPlayerClients STATIC
    src/mediaboxplayerclient.cpp
    src/mediaboxplayerclient.h
    src/mediaboxvplayerclient.cpp
    src/mediaboxvplayerclient.h
)
mediabox_manager_library(MediaBoxPlayerClients)

add_library(MediaBoxManagerApplication STATIC
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
    src/informer.cpp
    src/informer.h
    src/mediamanager.cpp
    src/mediamanager.h
    src/mediamodel.cpp
    src/mediamodel.h
    src/settings.cpp
    src/settings.h
    src/stationmanager.cpp
    src/stationmanager.h
    src/localplayerlauncher.cpp
    src/localplayerlauncher.h
    src/mediacontroller.cpp
    src/mediacontroller.h
    src/videocontroller.cpp
    src/videocontroller.h
    src/mediaboxmanagerdata.h
)
mediabox_manager_library(MediaBoxManagerApplication)

target_link_libraries(MediaBoxScheduleCore PUBLIC Qt6::Core)
target_link_libraries(MediaBoxScheduleStorage PUBLIC MediaBoxScheduleCore)
target_link_libraries(MediaBoxMediaLibrary PUBLIC Qt6::Core PRIVATE MediaBoxManager::TagLib)
target_link_libraries(MediaBoxPlayerClients PUBLIC Qt6::Core Qt6::Network)
target_link_libraries(MediaBoxManagerApplication PUBLIC
    MediaBoxScheduleStorage MediaBoxMediaLibrary
    MediaBoxPlayerClients MediaBoxPaths
)
target_compile_definitions(MediaBoxManagerApplication PUBLIC VERSION="${PROJECT_VERSION}")

# An object library keeps resources linked into both the application and tests.
add_library(MediaBoxManagerUi OBJECT
    src/aboutmediaboxmanager.cpp
    src/aboutmediaboxmanager.h
    src/composition.cpp
    src/composition.h
    src/compositionslist.cpp
    src/compositionslist.h
    src/restyletheme.cpp
    src/restyletheme.h
    src/restylewidgets.cpp
    src/restylewidgets.h
    src/restyleicons.qrc
    src/schedulepreview.cpp
    src/schedulepreview.h
    src/ruleeditors.cpp
    src/ruleeditors.h
    src/medialibrarydelegate.cpp
    src/medialibrarydelegate.h
    src/mainwindow.cpp
    src/mainwindow.h
    src/videocontrolwidget.cpp
    src/videocontrolwidget.h
    src/playercontrolwidget.cpp
    src/playercontrolwidget.h
    src/report.cpp
    src/report.h
    src/settingsdialog.cpp
    src/settingsdialog.h
    src/trialmessagebox.cpp
    src/trialmessagebox.h
    src/icons.qrc
)
set_target_properties(MediaBoxManagerUi PROPERTIES AUTORCC ON)
target_compile_features(MediaBoxManagerUi PUBLIC cxx_std_23)
target_include_directories(MediaBoxManagerUi PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_definitions(MediaBoxManagerUi PUBLIC
    VERSION="${PROJECT_VERSION}"
    "$<$<CONFIG:Debug>:SHOW_DEBUG_OUTPUT>"
)
target_link_libraries(MediaBoxManagerUi PUBLIC
    MediaBoxManagerApplication Qt6::Gui Qt6::Widgets
)
if(WIN32)
    target_link_libraries(MediaBoxManagerUi PUBLIC shell32)
endif()
if(MSVC)
    target_compile_options(MediaBoxManagerUi PUBLIC /utf-8 /permissive- /Zc:__cplusplus)
    target_compile_options(MediaBoxManagerUi PRIVATE /W4)
else()
    target_compile_options(MediaBoxManagerUi PRIVATE -Wall -Wextra)
endif()

qt_add_executable(MediaBoxManager WIN32 MACOSX_BUNDLE src/main.cpp)
target_link_libraries(MediaBoxManager PRIVATE MediaBoxManagerUi)
if(WIN32)
    target_sources(MediaBoxManager PRIVATE src/app.rc)
    set_source_files_properties(src/app.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/icons/app.ico")
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
target_link_libraries(MediaBoxManagerApplication PUBLIC MediaBoxPlayerCore)
add_subdirectory(MediaBoxVPlayer)
add_dependencies(MediaBoxManager MediaBoxPlayer MediaBoxVPlayer)
