find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(MediaBoxManagerTests tst_migration.cpp)
target_link_libraries(MediaBoxManagerTests PRIVATE MediaBoxManagerCore Qt6::Test)
set_target_properties(MediaBoxManagerTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_test(NAME MediaBoxManager_migration COMMAND "${CMAKE_COMMAND}"
    "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManagerTests>"
    "-DTEST_REPORT=${CMAKE_CURRENT_BINARY_DIR}/migration-results.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_test.cmake")
set_tests_properties(MediaBoxManager_migration PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

qt_add_executable(MediaBoxManagerDistributionTests tst_distribution.cpp)
target_link_libraries(MediaBoxManagerDistributionTests PRIVATE MediaBoxManagerCore Qt6::Test)
add_test(NAME MediaBoxManager_distribution COMMAND "${CMAKE_COMMAND}"
    "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManagerDistributionTests>"
    "-DTEST_REPORT=${CMAKE_CURRENT_BINARY_DIR}/distribution-results.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_test.cmake")
set_tests_properties(MediaBoxManager_distribution PROPERTIES TIMEOUT 30)

qt_add_executable(MediaBoxManagerStationTests tst_stationmanager.cpp)
target_link_libraries(MediaBoxManagerStationTests PRIVATE MediaBoxManagerCore Qt6::Test)
add_test(NAME MediaBoxManager_stationmanager COMMAND "${CMAKE_COMMAND}"
    "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManagerStationTests>"
    "-DTEST_REPORT=${CMAKE_CURRENT_BINARY_DIR}/stationmanager-results.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_test.cmake")
set_tests_properties(MediaBoxManager_stationmanager PROPERTIES TIMEOUT 30)

qt_add_executable(MediaBoxManagerSettingsTests tst_settings.cpp)
target_link_libraries(MediaBoxManagerSettingsTests PRIVATE MediaBoxManagerCore Qt6::Test)
add_test(NAME MediaBoxManager_settings COMMAND "${CMAKE_COMMAND}"
    "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManagerSettingsTests>"
    "-DTEST_REPORT=${CMAKE_CURRENT_BINARY_DIR}/settings-results.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_test.cmake")
set_tests_properties(MediaBoxManager_settings PROPERTIES TIMEOUT 30)

add_test(NAME MediaBoxManager_version COMMAND MediaBoxManager version)
set_tests_properties(MediaBoxManager_version PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}"
    TIMEOUT 10
)

if(WIN32)
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin "${_qt_qmake}" DIRECTORY)
    set_property(TEST MediaBoxManager_migration MediaBoxManager_distribution MediaBoxManager_stationmanager MediaBoxManager_settings MediaBoxManager_version APPEND PROPERTY
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
endif()

foreach(_suite IN ITEMS restyle schedulepreview ruleeditors secondary)
    qt_add_executable(MediaBoxManager_${_suite}Tests tst_${_suite}.cpp)
    target_link_libraries(MediaBoxManager_${_suite}Tests PRIVATE MediaBoxManagerCore Qt6::Test)
    add_test(NAME MediaBoxManager_${_suite} COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManager_${_suite}Tests>"
        "-DTEST_REPORT=${CMAKE_CURRENT_BINARY_DIR}/${_suite}-results.txt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_test.cmake")
    set_tests_properties(MediaBoxManager_${_suite} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    if(WIN32)
        set_property(TEST MediaBoxManager_${_suite} APPEND PROPERTY
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
    endif()
endforeach()

foreach(_suite IN ITEMS playerclient playerui)
    qt_add_executable(MediaBoxManager_${_suite}Tests tst_${_suite}.cpp)
    target_link_libraries(MediaBoxManager_${_suite}Tests PRIVATE MediaBoxManagerCore MediaBoxPlayerCore Qt6::Test)
    add_test(NAME MediaBoxManager_${_suite} COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXECUTABLE=$<TARGET_FILE:MediaBoxManager_${_suite}Tests>"
        "-DTEST_REPORT=${CMAKE_CURRENT_BINARY_DIR}/${_suite}-results.txt"
        "-DTEST_TIMEOUT=110"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_test.cmake")
    set_tests_properties(MediaBoxManager_${_suite} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    if(WIN32)
        set_property(TEST MediaBoxManager_${_suite} APPEND PROPERTY
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
    endif()
endforeach()

target_compile_definitions(MediaBoxManager_playerclientTests PRIVATE
    PLAYER_EXECUTABLE="$<TARGET_FILE:MediaBoxPlayer>")
add_dependencies(MediaBoxManager_playerclientTests MediaBoxPlayer)
