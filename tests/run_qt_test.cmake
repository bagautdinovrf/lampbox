# QtTest on Windows can send its default logger to the debugger. Always capture
# a text report so CTest --output-on-failure includes the actual failing check.
if(NOT DEFINED TEST_EXECUTABLE OR NOT DEFINED TEST_REPORT)
    message(FATAL_ERROR "TEST_EXECUTABLE and TEST_REPORT are required")
endif()
file(REMOVE "${TEST_REPORT}")
execute_process(
    COMMAND "${TEST_EXECUTABLE}" -o "${TEST_REPORT},txt"
    RESULT_VARIABLE test_result
    TIMEOUT 25
)
if(EXISTS "${TEST_REPORT}")
    file(READ "${TEST_REPORT}" test_output)
    message("${test_output}")
endif()
if(NOT "${test_result}" STREQUAL "0")
    message(FATAL_ERROR "QtTest failed: ${test_result}")
endif()
