include(FetchContent)

set(LAMPBOX_TAGLIB_SOURCE_DIR "" CACHE PATH
    "Path to an unpacked TagLib 1.13.1 source tree for offline builds")

# Keep dependency options scoped so its BUILD_TESTING does not disable our tests.
function(lampbox_add_taglib)
    # CMake 4 can still configure the older dependency without patching its source.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_BINDINGS OFF)
    set(ENABLE_STATIC_RUNTIME OFF)
    set(WITH_ZLIB OFF)
    set(CMAKE_AUTOMOC OFF)
    set(CMAKE_AUTOUIC OFF)
    set(CMAKE_AUTORCC OFF)

    if(LAMPBOX_TAGLIB_SOURCE_DIR)
        if(NOT EXISTS "${LAMPBOX_TAGLIB_SOURCE_DIR}/taglib/toolkit/taglib.h")
            message(FATAL_ERROR "LAMPBOX_TAGLIB_SOURCE_DIR must point to TagLib sources")
        endif()
        file(STRINGS "${LAMPBOX_TAGLIB_SOURCE_DIR}/taglib/toolkit/taglib.h"
            taglib_version REGEX "^#define TAGLIB_(MAJOR|MINOR|PATCH)_VERSION")
        if(NOT taglib_version MATCHES "TAGLIB_MAJOR_VERSION 1;#define TAGLIB_MINOR_VERSION 13;#define TAGLIB_PATCH_VERSION 1$")
            message(FATAL_ERROR "LampBox requires the pinned TagLib 1.13.1 sources")
        endif()
        set(FETCHCONTENT_SOURCE_DIR_TAGLIB "${LAMPBOX_TAGLIB_SOURCE_DIR}")
    endif()

    FetchContent_Declare(taglib
        URL https://codeload.github.com/taglib/taglib/tar.gz/refs/tags/v1.13.1
        URL_HASH SHA256=c8da2b10f1bfec2cd7dbfcd33f4a2338db0765d851a50583d410bacf055cfd0b
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(taglib)

    # TagLib 1.x does not export usage requirements for its in-tree target.
    add_library(lampbox_taglib INTERFACE)
    add_library(LampBox::TagLib ALIAS lampbox_taglib)
    target_link_libraries(lampbox_taglib INTERFACE tag)
    target_compile_definitions(lampbox_taglib INTERFACE TAGLIB_STATIC)
    target_include_directories(lampbox_taglib SYSTEM INTERFACE
        "${taglib_SOURCE_DIR}/taglib"
        "${taglib_SOURCE_DIR}/taglib/toolkit"
        "${taglib_BINARY_DIR}"
    )
endfunction()

lampbox_add_taglib()
