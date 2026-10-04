# Shared by the full suite and standalone player builds.
set(MEDIABOX_VERSION_FILE "${CMAKE_CURRENT_LIST_DIR}/../VERSION.txt")
file(READ "${MEDIABOX_VERSION_FILE}" MEDIABOX_VERSION)
string(STRIP "${MEDIABOX_VERSION}" MEDIABOX_VERSION)
if(NOT MEDIABOX_VERSION MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
    message(FATAL_ERROR "VERSION.txt must contain a version in major.minor.patch format")
endif()
# Reconfigure an existing build when only the release number changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MEDIABOX_VERSION_FILE}")
