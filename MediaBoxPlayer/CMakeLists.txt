cmake_minimum_required(VERSION 3.28)

project(MediaBoxPlayer VERSION 0.1.0 LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    include(CTest)
    include(GNUInstallDirs)
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Core)
if(PROJECT_IS_TOP_LEVEL)
    qt_standard_project_setup(REQUIRES 6.12)
endif()

qt_add_executable(MediaBoxPlayer src/main.cpp)
target_compile_features(MediaBoxPlayer PRIVATE cxx_std_23)
target_compile_definitions(MediaBoxPlayer PRIVATE
    MEDIABOXPLAYER_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(MediaBoxPlayer PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(MediaBoxPlayer PRIVATE /utf-8 /permissive- /Zc:__cplusplus /W4)
else()
    target_compile_options(MediaBoxPlayer PRIVATE -Wall -Wextra)
endif()

install(TARGETS MediaBoxPlayer
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
qt_generate_deploy_app_script(
    TARGET MediaBoxPlayer
    OUTPUT_SCRIPT MediaBoxPlayer_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT "${MediaBoxPlayer_deploy_script}")

if(BUILD_TESTING)
    add_test(NAME MediaBoxPlayer_version COMMAND MediaBoxPlayer --version)
    string(REPLACE "." "\\." _player_version_regex "${PROJECT_VERSION}")
    set_tests_properties(MediaBoxPlayer_version PROPERTIES
        PASS_REGULAR_EXPRESSION "MediaBoxPlayer ${_player_version_regex}"
        TIMEOUT 10
    )
    if(WIN32)
        get_target_property(_player_qmake Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(_player_qt_bin "${_player_qmake}" DIRECTORY)
        set_property(TEST MediaBoxPlayer_version APPEND PROPERTY
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_player_qt_bin}")
    endif()
endif()
