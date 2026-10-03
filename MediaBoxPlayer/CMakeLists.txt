cmake_minimum_required(VERSION 3.28)

project(MediaBoxPlayer VERSION 0.2.0 LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    include(CTest)
    include(GNUInstallDirs)
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Network Multimedia)
if(PROJECT_IS_TOP_LEVEL)
    qt_standard_project_setup(REQUIRES 6.12)
endif()

if(NOT TARGET MediaBoxPaths)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common" "${CMAKE_CURRENT_BINARY_DIR}/common")
endif()

add_library(MediaBoxPlayerCore STATIC
    src/audiobackend.cpp src/audiobackend.h
    src/playerengine.cpp src/playerengine.h
    src/controlserver.cpp src/controlserver.h
    src/desktopservice.cpp src/desktopservice.h
)
target_include_directories(MediaBoxPlayerCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(MediaBoxPlayerCore PUBLIC cxx_std_23)
target_link_libraries(MediaBoxPlayerCore PUBLIC Qt6::Core Qt6::Network)
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

if(BUILD_TESTING AND NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(MediaBoxPlayerEngineTests tests/tst_playerengine.cpp)
    target_link_libraries(MediaBoxPlayerEngineTests PRIVATE MediaBoxPlayerCore Qt6::Test)
    add_test(NAME MediaBoxPlayer_engine COMMAND MediaBoxPlayerEngineTests)
    set_tests_properties(MediaBoxPlayer_engine PROPERTIES TIMEOUT 30)

    qt_add_executable(MediaBoxPlayerControlTests tests/tst_controlserver.cpp)
    target_link_libraries(MediaBoxPlayerControlTests PRIVATE MediaBoxPlayerCore Qt6::Test)
    add_test(NAME MediaBoxPlayer_control COMMAND MediaBoxPlayerControlTests)
    set_tests_properties(MediaBoxPlayer_control PROPERTIES TIMEOUT 30)

    qt_add_executable(MediaBoxPlayerProcessTests tests/tst_process.cpp)
    target_link_libraries(MediaBoxPlayerProcessTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
    target_compile_definitions(MediaBoxPlayerProcessTests PRIVATE
        PLAYER_EXECUTABLE="$<TARGET_FILE:MediaBoxPlayer>")
    add_dependencies(MediaBoxPlayerProcessTests MediaBoxPlayer)
    add_test(NAME MediaBoxPlayer_process COMMAND MediaBoxPlayerProcessTests)
    set_tests_properties(MediaBoxPlayer_process PROPERTIES TIMEOUT 30)

    add_test(NAME MediaBoxPlayer_version COMMAND MediaBoxPlayer --version)
    string(REPLACE "." "\\." _player_version_regex "${PROJECT_VERSION}")
    set_tests_properties(MediaBoxPlayer_version PROPERTIES
        PASS_REGULAR_EXPRESSION "MediaBoxPlayer ${_player_version_regex}"
        TIMEOUT 10
    )
    if(WIN32)
        get_target_property(_player_qmake Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(_player_qt_bin "${_player_qmake}" DIRECTORY)
        set_property(TEST MediaBoxPlayer_version MediaBoxPlayer_engine MediaBoxPlayer_control MediaBoxPlayer_process APPEND PROPERTY
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_player_qt_bin}")
    endif()
endif()
