include(FetchContent)

set(LAMPBOX_TAGLIB_SOURCE_DIR "" CACHE PATH
    "Path to an unpacked TagLib 2.3.2 release archive for offline builds")

# Keep dependency options scoped so its BUILD_TESTING does not disable our tests.
function(lampbox_add_taglib)
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
        if(NOT taglib_version MATCHES "TAGLIB_MAJOR_VERSION 2;#define TAGLIB_MINOR_VERSION 3;#define TAGLIB_PATCH_VERSION 2$")
            message(FATAL_ERROR "MediaBoxManager requires the pinned TagLib 2.3.2 sources")
        endif()
        set(FETCHCONTENT_SOURCE_DIR_TAGLIB "${LAMPBOX_TAGLIB_SOURCE_DIR}")
    endif()

    # The release archive includes utf8cpp, unlike GitHub's generated source archives.
    FetchContent_Declare(taglib
        URL https://github.com/taglib/taglib/releases/download/v2.3.2/taglib-2.3.2.tar.gz
        URL_HASH SHA256=3ca2d8afaa7f1cf7f6ed10e511ebc368bfacd6dcaa3dbfa690b89e502e8963dc
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(taglib)

    # TagLib does not export build-tree include directories for its in-tree target.
    add_library(lampbox_taglib INTERFACE)
    add_library(LampBox::TagLib ALIAS lampbox_taglib)
    target_link_libraries(lampbox_taglib INTERFACE tag)
    target_include_directories(lampbox_taglib SYSTEM INTERFACE
        "${taglib_SOURCE_DIR}/taglib"
        "${taglib_SOURCE_DIR}/taglib/toolkit"
        "${taglib_BINARY_DIR}"
    )
endfunction()

lampbox_add_taglib()
