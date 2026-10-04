# Injected only by agent_build/build.ps1 -WithTests. Production CMake files
# never include this module or define test targets.
include_guard(GLOBAL)

# Keep this in root scope so child directories also refresh their CTest files.
enable_testing()

function(mediaboxmanager_configure_agent_tests)
    if(ANDROID)
        message(FATAL_ERROR "agent_build tests require a desktop build")
    endif()
    # This function runs in the root directory after all application targets
    # exist, keeping the normal application build and its cache intact.
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests.cmake")
endfunction()

cmake_language(DEFER CALL mediaboxmanager_configure_agent_tests)
