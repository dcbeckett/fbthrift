# This will go away as soon as Proxygen switches over to cmake.
#
# Hints:
#   Set PROXYGEN_ROOT_DIR to the installation prefix of proxygen
#
# Results:
#   Sets proxygen_FOUND and defines the proxygen::proxygen TARGET

include(FindPackageHandleStandardArgs)

find_path(
    PROXYGEN_INCLUDE_DIR
    NAMES
        proxygen/lib/http/HTTPMessage.h
    HINTS
        ${PROXYGEN_ROOT_DIR}
)

find_library(
    PROXYGEN_LIBRARY
    NAMES
        libproxygenlib.a
    HINTS
        ${PROXYGEN_ROOT_DIR}
)

find_library(
    PROXYGEN_LIBRARY_HTTPSERVER
    NAMES
        libproxygenhttpserver.a
    HINTS
        ${PROXYGEN_ROOT_DIR}
)

find_package_handle_standard_args(proxygen
    REQUIRED_VARS
        PROXYGEN_INCLUDE_DIR
        PROXYGEN_LIBRARY
        PROXYGEN_LIBRARY_HTTPSERVER
    FAIL_MESSAGE
        "Could not find proxygen. Try setting the proxygen installation prefix with PROXYGEN_ROOT_DIR"
)
if(proxygen_FOUND)
    if(NOT TARGET proxygen::lib)
        add_library(proxygen::lib STATIC IMPORTED)
        set_target_properties(proxygen::lib PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            IMPORTED_LOCATION "${PROXYGEN_LIBRARY}"
        )
        set_property(
            TARGET proxygen::lib
            APPEND
            PROPERTY INTERFACE_LINK_LIBRARIES
                Folly::folly
                wangle::wangle
                fizz::fizz
        )
    endif()

    if (NOT TARGET proxygen::httpserver)
        add_library(proxygen::httpserver STATIC IMPORTED)
        set_target_properties(proxygen::httpserver PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            IMPORTED_LOCATION "${PROXYGEN_LIBRARY_HTTPSERVER}"
            INTERFACE_LINK_LIBRARIES proxygen::lib
        )
    endif()

    if (NOT TARGET proxygen::proxygen)
        add_library(proxygen::proxygen INTERFACE IMPORTED)
        set_property(TARGET proxygen::proxygen APPEND PROPERTY INTERFACE_LINK_LIBRARIES proxygen::httpserver proxygen::lib)
        get_target_property(things proxygen::proxygen INTERFACE_LINK_LIBRARIES)
        set_target_properties(
            proxygen::proxygen
            PROPERTIES
             INTERFACE_INCLUDE_DIRECTORIES
                ${PROXYGEN_INCLUDE_DIR}
        )
    endif()

    message("Found proxygen")
endif()

