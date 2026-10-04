cmake_minimum_required(VERSION 3.28)
project(MediaBoxVPlayer VERSION 0.1.0 LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    include(GNUInstallDirs)
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia MultimediaWidgets)
if(PROJECT_IS_TOP_LEVEL)
    qt_standard_project_setup(REQUIRES 6.12)
endif()
if(NOT TARGET MediaBoxPlayerCore)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../MediaBoxPlayer" "${CMAKE_CURRENT_BINARY_DIR}/MediaBoxPlayer")
endif()

add_library(MediaBoxVPlayerCore STATIC
    src/videoservice.cpp src/videoservice.h
    src/videowindow.cpp src/videowindow.h
    src/qtvideobackend.cpp src/qtvideobackend.h
)
target_include_directories(MediaBoxVPlayerCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(MediaBoxVPlayerCore PUBLIC cxx_std_23)
target_link_libraries(MediaBoxVPlayerCore PUBLIC MediaBoxPlayerCore Qt6::Widgets Qt6::MultimediaWidgets)

qt_add_executable(MediaBoxVPlayer WIN32 MACOSX_BUNDLE src/main.cpp)
target_compile_definitions(MediaBoxVPlayer PRIVATE MEDIABOXVPLAYER_VERSION="${PROJECT_VERSION}")
target_link_libraries(MediaBoxVPlayer PRIVATE MediaBoxVPlayerCore MediaBoxPaths)
if(MSVC)
    target_compile_options(MediaBoxVPlayerCore PUBLIC /utf-8 /permissive- /Zc:__cplusplus)
    target_compile_options(MediaBoxVPlayerCore PRIVATE /W4)
    target_compile_options(MediaBoxVPlayer PRIVATE /W4)
else()
    target_compile_options(MediaBoxVPlayerCore PRIVATE -Wall -Wextra)
    target_compile_options(MediaBoxVPlayer PRIVATE -Wall -Wextra)
endif()

install(TARGETS MediaBoxVPlayer
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
qt_generate_deploy_app_script(TARGET MediaBoxVPlayer OUTPUT_SCRIPT MediaBoxVPlayer_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT "${MediaBoxVPlayer_deploy_script}")
install(FILES README.md TCP_API.md DESTINATION "${CMAKE_INSTALL_DATADIR}/MediaBoxVPlayer")
