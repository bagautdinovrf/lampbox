cmake_minimum_required(VERSION 3.28)

project(MediaBoxPlayer VERSION 0.3.0 LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    include(GNUInstallDirs)
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Network Multimedia Sql)
if(PROJECT_IS_TOP_LEVEL)
    qt_standard_project_setup(REQUIRES 6.12)
endif()

if(NOT TARGET MediaBoxPaths)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common" "${CMAKE_CURRENT_BINARY_DIR}/common")
endif()

if(NOT TARGET MediaBoxScheduleCore)
    add_library(MediaBoxScheduleCore STATIC
        ../src/schedulecore/schedulecore.cpp ../src/schedulecore/schedulecore.h
        ../src/schedulecore/schedulev1.cpp ../src/schedulecore/schedulev1.h)
    target_include_directories(MediaBoxScheduleCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../src")
    target_link_libraries(MediaBoxScheduleCore PUBLIC Qt6::Core)
    target_compile_features(MediaBoxScheduleCore PUBLIC cxx_std_23)
    if(MSVC)
        target_compile_options(MediaBoxScheduleCore PRIVATE /utf-8)
    endif()
endif()

add_library(MediaBoxPlayerCore STATIC
    src/audiobackend.cpp src/audiobackend.h
    src/playerengine.cpp src/playerengine.h
    src/playbackschedule.cpp src/playbackschedule.h
    src/controlserver.cpp src/controlserver.h
    src/desktopservice.cpp src/desktopservice.h
    src/schedulev1runtime.cpp src/schedulev1runtime.h
)
target_include_directories(MediaBoxPlayerCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(MediaBoxPlayerCore PUBLIC cxx_std_23)
target_link_libraries(MediaBoxPlayerCore PUBLIC MediaBoxScheduleCore Qt6::Core Qt6::Network Qt6::Sql)
if(WIN32)
    target_link_libraries(MediaBoxPlayerCore PRIVATE advapi32)
endif()

qt_add_executable(MediaBoxPlayer
    src/main.cpp
    src/qtaudiobackend.cpp src/qtaudiobackend.h
    src/androidservice.cpp src/androidservice.h
)
target_compile_definitions(MediaBoxPlayer PRIVATE
    MEDIABOXPLAYER_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(MediaBoxPlayer PRIVATE MediaBoxPlayerCore MediaBoxPaths Qt6::Multimedia)
if(ANDROID)
    target_link_libraries(MediaBoxPlayer PRIVATE Qt6::CorePrivate)
    set_target_properties(MediaBoxPlayer PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "org.mediabox.player"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36
    )
endif()
if(MSVC)
    target_compile_options(MediaBoxPlayerCore PUBLIC /utf-8 /permissive- /Zc:__cplusplus)
    target_compile_options(MediaBoxPlayerCore PRIVATE /W4)
    target_compile_options(MediaBoxPlayer PRIVATE /W4)
else()
    target_compile_options(MediaBoxPlayerCore PRIVATE -Wall -Wextra)
    target_compile_options(MediaBoxPlayer PRIVATE -Wall -Wextra)
endif()

if(NOT ANDROID)
    install(TARGETS MediaBoxPlayer
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    qt_generate_deploy_app_script(
        TARGET MediaBoxPlayer
        OUTPUT_SCRIPT MediaBoxPlayer_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT "${MediaBoxPlayer_deploy_script}")
    install(DIRECTORY deploy/ DESTINATION "${CMAKE_INSTALL_DATADIR}/MediaBoxPlayer/deploy")
endif()
