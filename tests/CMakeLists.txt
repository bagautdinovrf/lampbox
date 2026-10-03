find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(lampbox_tests tst_migration.cpp)
target_link_libraries(lampbox_tests PRIVATE lampbox_core Qt6::Test)
set_target_properties(lampbox_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_test(NAME lampbox_migration COMMAND "${CMAKE_COMMAND}"
    "-DTEST_EXECUTABLE=$<TARGET_FILE:lampbox_tests>"
    "-DTEST_REPORT=${CMAKE_CURRENT_BINARY_DIR}/migration-results.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_test.cmake")
set_tests_properties(lampbox_migration PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

qt_add_executable(lampbox_distribution_tests tst_distribution.cpp)
target_link_libraries(lampbox_distribution_tests PRIVATE lampbox_core Qt6::Test)
add_test(NAME lampbox_distribution COMMAND "${CMAKE_COMMAND}"
    "-DTEST_EXECUTABLE=$<TARGET_FILE:lampbox_distribution_tests>"
    "-DTEST_REPORT=${CMAKE_CURRENT_BINARY_DIR}/distribution-results.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_test.cmake")
set_tests_properties(lampbox_distribution PROPERTIES TIMEOUT 30)

add_test(NAME lampbox_version COMMAND lampbox version)
set_tests_properties(lampbox_version PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}"
    TIMEOUT 10
)

if(WIN32)
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin "${_qt_qmake}" DIRECTORY)
    set_property(TEST lampbox_migration lampbox_distribution lampbox_version APPEND PROPERTY
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin}")
endif()
