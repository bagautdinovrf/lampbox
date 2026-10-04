# Player Qt targets were found in child directories; expose them here too for
# Qt's finalization of tests linked to the video player library.
find_package(Qt6 REQUIRED COMPONENTS Test Multimedia MultimediaWidgets)
get_property(_application_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
set(_test_report_directory "${CMAKE_BINARY_DIR}/tests")
file(MAKE_DIRECTORY "${_test_report_directory}")
set(_manager_test_source "${CMAKE_SOURCE_DIR}/tests")
set(_player_test_source "${CMAKE_SOURCE_DIR}/MediaBoxPlayer/tests")
set(_vplayer_test_source "${CMAKE_SOURCE_DIR}/MediaBoxVPlayer/tests")

qt_add_executable(MediaBoxManagerTests "${_manager_test_source}/tst_migration.cpp")
target_link_libraries(MediaBoxManagerTests PRIVATE MediaBoxManagerUi MediaBoxManager::TagLib Qt6::Test)
set_target_properties(MediaBoxManagerTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_test(NAME MediaBoxManager_migration COMMAND "${CMAKE_COMMAND}"
    "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManagerTests>"
    "-DTEST_REPORT=${_test_report_directory}/migration-results.txt"
    -P "${CMAKE_CURRENT_LIST_DIR}/run_qt_test.cmake")
set_tests_properties(MediaBoxManager_migration PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

qt_add_executable(MediaBoxManagerStationTests "${_manager_test_source}/tst_stationmanager.cpp")
target_link_libraries(MediaBoxManagerStationTests PRIVATE MediaBoxManagerApplication Qt6::Test)
add_test(NAME MediaBoxManager_stationmanager COMMAND "${CMAKE_COMMAND}"
    "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManagerStationTests>"
    "-DTEST_REPORT=${_test_report_directory}/stationmanager-results.txt"
    -P "${CMAKE_CURRENT_LIST_DIR}/run_qt_test.cmake")
set_tests_properties(MediaBoxManager_stationmanager PROPERTIES TIMEOUT 30)

qt_add_executable(MediaBoxManagerSettingsTests "${_manager_test_source}/tst_settings.cpp")
target_link_libraries(MediaBoxManagerSettingsTests PRIVATE MediaBoxManagerApplication Qt6::Test)
add_test(NAME MediaBoxManager_settings COMMAND "${CMAKE_COMMAND}"
    "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManagerSettingsTests>"
    "-DTEST_REPORT=${_test_report_directory}/settings-results.txt"
    -P "${CMAKE_CURRENT_LIST_DIR}/run_qt_test.cmake")
set_tests_properties(MediaBoxManager_settings PROPERTIES TIMEOUT 30)

add_test(NAME MediaBoxManager_version COMMAND MediaBoxManager version)
set_tests_properties(MediaBoxManager_version PROPERTIES
    WORKING_DIRECTORY "${_test_report_directory}"
    PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}"
    TIMEOUT 10
)

if(WIN32)
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin "${_qt_qmake}" DIRECTORY)
    set_property(TEST MediaBoxManager_migration MediaBoxManager_stationmanager MediaBoxManager_settings MediaBoxManager_version APPEND PROPERTY
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
endif()

foreach(_suite IN ITEMS restyle schedulepreview ruleeditors secondary playbackui)
    qt_add_executable(MediaBoxManager_${_suite}Tests "${_manager_test_source}/tst_${_suite}.cpp")
    target_link_libraries(MediaBoxManager_${_suite}Tests PRIVATE MediaBoxManagerUi Qt6::Test)
    add_test(NAME MediaBoxManager_${_suite} COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManager_${_suite}Tests>"
        "-DTEST_REPORT=${_test_report_directory}/${_suite}-results.txt"
        -P "${CMAKE_CURRENT_LIST_DIR}/run_qt_test.cmake")
    set_tests_properties(MediaBoxManager_${_suite} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    if(WIN32)
        set_property(TEST MediaBoxManager_${_suite} APPEND PROPERTY
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
    endif()
endforeach()

foreach(_suite IN ITEMS playerclient playerui playerautostart)
    qt_add_executable(MediaBoxManager_${_suite}Tests "${_manager_test_source}/tst_${_suite}.cpp")
    if(_suite STREQUAL "playerui")
        target_link_libraries(MediaBoxManager_${_suite}Tests PRIVATE MediaBoxManagerUi Qt6::Test)
    else()
        target_link_libraries(MediaBoxManager_${_suite}Tests PRIVATE MediaBoxManagerApplication Qt6::Test)
    endif()
    add_test(NAME MediaBoxManager_${_suite} COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManager_${_suite}Tests>"
        "-DTEST_REPORT=${_test_report_directory}/${_suite}-results.txt"
        "-DTEST_TIMEOUT=110"
        -P "${CMAKE_CURRENT_LIST_DIR}/run_qt_test.cmake")
    set_tests_properties(MediaBoxManager_${_suite} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    if(WIN32)
        set_property(TEST MediaBoxManager_${_suite} APPEND PROPERTY
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
    endif()
endforeach()

target_compile_definitions(MediaBoxManager_playerclientTests PRIVATE
    PLAYER_EXECUTABLE="$<TARGET_FILE:MediaBoxPlayer>")
add_dependencies(MediaBoxManager_playerclientTests MediaBoxPlayer)
add_dependencies(MediaBoxManager_playerautostartTests MediaBoxPlayer)

qt_add_executable(MediaBoxVPlayerProcessTests "${_manager_test_source}/tst_vplayerprocess.cpp")
target_link_libraries(MediaBoxVPlayerProcessTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
target_compile_definitions(MediaBoxVPlayerProcessTests PRIVATE VPLAYER_EXECUTABLE="$<TARGET_FILE:MediaBoxVPlayer>")
if(MSVC)
    target_compile_options(MediaBoxVPlayerProcessTests PRIVATE /utf-8)
endif()
add_dependencies(MediaBoxVPlayerProcessTests MediaBoxVPlayer)
add_test(NAME MediaBoxVPlayer_process COMMAND MediaBoxVPlayerProcessTests)
set_tests_properties(MediaBoxVPlayer_process PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
if(WIN32)
    set_property(TEST MediaBoxVPlayer_process APPEND PROPERTY
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
endif()

foreach(_suite IN ITEMS vplayerclient videocontrols vplayerautostart)
    qt_add_executable(MediaBoxManager_${_suite}Tests "${_manager_test_source}/tst_${_suite}.cpp")
    if(_suite STREQUAL "videocontrols")
        target_link_libraries(MediaBoxManager_${_suite}Tests PRIVATE MediaBoxManagerUi Qt6::Test)
    else()
        target_link_libraries(MediaBoxManager_${_suite}Tests PRIVATE MediaBoxManagerApplication Qt6::Test)
    endif()
    add_test(NAME MediaBoxManager_${_suite} COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManager_${_suite}Tests>"
        "-DTEST_REPORT=${_test_report_directory}/${_suite}-results.txt"
        "-DTEST_TIMEOUT=110"
        -P "${CMAKE_CURRENT_LIST_DIR}/run_qt_test.cmake")
    set_tests_properties(MediaBoxManager_${_suite} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    if(WIN32)
        set_property(TEST MediaBoxManager_${_suite} APPEND PROPERTY
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
    endif()
endforeach()

add_dependencies(MediaBoxManager_vplayerautostartTests MediaBoxVPlayer)

# These suites deliberately have no Qt Widgets/Multimedia dependency.
foreach(_suite IN ITEMS schedulecore schedulepersistence mediaimport schedulev1 schedulepublication schedulev1runtime)
    qt_add_executable(MediaBoxManager_${_suite}Tests "${_manager_test_source}/tst_${_suite}.cpp")
    if(_suite STREQUAL "schedulecore" OR _suite STREQUAL "schedulev1")
        target_link_libraries(MediaBoxManager_${_suite}Tests PRIVATE MediaBoxScheduleCore Qt6::Test)
    elseif(_suite STREQUAL "schedulev1runtime")
        target_link_libraries(MediaBoxManager_${_suite}Tests PRIVATE MediaBoxPlayerCore Qt6::Test)
    elseif(_suite STREQUAL "schedulepersistence")
        target_link_libraries(MediaBoxManager_${_suite}Tests PRIVATE MediaBoxManagerApplication Qt6::Test)
    else()
        target_link_libraries(MediaBoxManager_${_suite}Tests PRIVATE MediaBoxManagerApplication Qt6::Test)
    endif()
    add_test(NAME MediaBoxManager_${_suite} COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManager_${_suite}Tests>"
        "-DTEST_REPORT=${_test_report_directory}/${_suite}-results.txt"
        -P "${CMAKE_CURRENT_LIST_DIR}/run_qt_test.cmake")
    set_tests_properties(MediaBoxManager_${_suite} PROPERTIES TIMEOUT 60)
    if(WIN32)
        set_property(TEST MediaBoxManager_${_suite} APPEND PROPERTY
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
    endif()
endforeach()

qt_add_executable(MediaBoxPathsTests "${CMAKE_SOURCE_DIR}/common/tst_storagepaths.cpp")
target_link_libraries(MediaBoxPathsTests PRIVATE MediaBoxPaths Qt6::Test)
add_test(NAME MediaBox_storagepaths COMMAND MediaBoxPathsTests)
set_tests_properties(MediaBox_storagepaths PROPERTIES TIMEOUT 30)
if(WIN32)
    get_target_property(_paths_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_paths_qt_bin "${_paths_qmake}" DIRECTORY)
    set_property(TEST MediaBox_storagepaths APPEND PROPERTY
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_paths_qt_bin}")
endif()

qt_add_executable(MediaBoxPlayerEngineTests "${_player_test_source}/tst_playerengine.cpp")
target_link_libraries(MediaBoxPlayerEngineTests PRIVATE MediaBoxPlayerCore Qt6::Test)
add_test(NAME MediaBoxPlayer_engine COMMAND MediaBoxPlayerEngineTests)
set_tests_properties(MediaBoxPlayer_engine PROPERTIES TIMEOUT 30)

qt_add_executable(MediaBoxPlayerControlTests "${_player_test_source}/tst_controlserver.cpp")
target_link_libraries(MediaBoxPlayerControlTests PRIVATE MediaBoxPlayerCore Qt6::Test)
add_test(NAME MediaBoxPlayer_control COMMAND MediaBoxPlayerControlTests)
set_tests_properties(MediaBoxPlayer_control PROPERTIES TIMEOUT 30)

qt_add_executable(MediaBoxPlayerProcessTests "${_player_test_source}/tst_process.cpp")
target_link_libraries(MediaBoxPlayerProcessTests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
target_compile_definitions(MediaBoxPlayerProcessTests PRIVATE
    PLAYER_EXECUTABLE="$<TARGET_FILE:MediaBoxPlayer>")
add_dependencies(MediaBoxPlayerProcessTests MediaBoxPlayer)
add_test(NAME MediaBoxPlayer_process COMMAND MediaBoxPlayerProcessTests)
set_tests_properties(MediaBoxPlayer_process PROPERTIES TIMEOUT 30)

add_test(NAME MediaBoxPlayer_version COMMAND MediaBoxPlayer --version)
get_target_property(_player_definitions MediaBoxPlayer COMPILE_DEFINITIONS)
string(REGEX MATCH "MEDIABOXPLAYER_VERSION=\"([0-9.]+)\"" _player_version_definition "${_player_definitions}")
if(NOT _player_version_definition)
    message(FATAL_ERROR "MediaBoxPlayer version definition was not found")
endif()
string(REPLACE "." "\\." _player_version_regex "${CMAKE_MATCH_1}")
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

qt_add_executable(MediaBoxVPlayerServiceTests "${_vplayer_test_source}/tst_videoservice.cpp")
target_link_libraries(MediaBoxVPlayerServiceTests PRIVATE MediaBoxVPlayerCore Qt6::Test)
add_test(NAME MediaBoxVPlayer_service COMMAND MediaBoxVPlayerServiceTests)
set_tests_properties(MediaBoxVPlayer_service PROPERTIES TIMEOUT 45
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_test(NAME MediaBoxVPlayer_version COMMAND MediaBoxVPlayer --version)
set_tests_properties(MediaBoxVPlayer_version PROPERTIES TIMEOUT 10
    PASS_REGULAR_EXPRESSION "MediaBoxVPlayer 0\\.2\\.0"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
if(WIN32)
    get_target_property(_vplayer_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_vplayer_qt_bin "${_vplayer_qmake}" DIRECTORY)
    set_property(TEST MediaBoxVPlayer_service MediaBoxVPlayer_version APPEND PROPERTY
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_vplayer_qt_bin}")
endif()

# Test executables belong to an explicit script-driven build, never to ALL.
get_property(_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
list(REMOVE_ITEM _test_targets ${_application_targets})
add_custom_target(mediaboxmanager_tests)
foreach(_target IN LISTS _test_targets)
    get_target_property(_target_type ${_target} TYPE)
    if(_target_type STREQUAL "EXECUTABLE")
        set_target_properties(${_target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
        add_dependencies(mediaboxmanager_tests ${_target})
    endif()
endforeach()
