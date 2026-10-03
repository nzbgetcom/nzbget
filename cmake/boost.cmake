# Boost 1.91.0 - Header-only FetchContent
# We only need boost::json and boost::test (header-only)
# When BUILD_DEPS_FROM_SOURCE=OFF: tries system find_package first (local development)
# When BUILD_DEPS_FROM_SOURCE=ON: skips system lookup for hermetic builds

include(FetchContent)

set(BOOST_VERSION "1.91.0")
set(BOOST_SHA256 "5734305f40a76c30f951c9abd409a45a2a19fb546efe4162119250bbe4d3a463")
string(REPLACE "." "_" BOOST_VERSION_UNDERSCORE "${BOOST_VERSION}")
# Primary: official archives.boost.io, Fallback: GitHub releases
set(BOOST_URLS
    "https://archives.boost.io/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_UNDERSCORE}.tar.gz"
    "https://github.com/boostorg/boost/releases/download/boost-${BOOST_VERSION}/boost_${BOOST_VERSION_UNDERSCORE}.tar.gz"
)

if(NOT BUILD_DEPS_FROM_SOURCE)
    # Local development: try system Boost first
    find_package(Boost ${BOOST_VERSION} QUIET COMPONENTS json unit_test_framework)
    if(Boost_FOUND)
        message(STATUS "Using system Boost ${BOOST_VERSION} (found via find_package)")
        # System Boost provides targets Boost::headers, Boost::json, Boost::unit_test_framework
        list(APPEND EXTERNAL_DEPS Boost::headers Boost::json Boost::unit_test_framework)
        set(BOOST_FROM_SYSTEM 1)
    endif()
endif()

if(NOT BOOST_FROM_SYSTEM)
    message(STATUS "Building Boost ${BOOST_VERSION} via FetchContent (header-only)")

    FetchContent_Declare(boost
        URL ${BOOST_URLS}
        URL_HASH SHA256=${BOOST_SHA256}
    )

    FetchContent_MakeAvailable(boost)

    # The boost source is now at ${boost_SOURCE_DIR}
    # Create interface targets pointing to the unpacked headers
    add_library(Boost::headers INTERFACE IMPORTED GLOBAL)
    target_include_directories(Boost::headers INTERFACE ${boost_SOURCE_DIR})
    target_compile_definitions(Boost::headers INTERFACE BOOST_ALL_NO_LIB)

    add_library(Boost::boost INTERFACE IMPORTED GLOBAL)
    target_link_libraries(Boost::boost INTERFACE Boost::headers)

    add_library(Boost::unit_test_framework INTERFACE IMPORTED GLOBAL)
    target_link_libraries(Boost::unit_test_framework INTERFACE Boost::headers)

    add_library(Boost::json INTERFACE IMPORTED GLOBAL)
    target_link_libraries(Boost::json INTERFACE Boost::headers)

    # Boost is header-only - no build target to depend on
endif()

# nzbget uses header-only Boost (boost::json via src.hpp and boost::test via included/unit_test.hpp).
# Disable MSVC auto-linking (#pragma comment(lib, ...)) across all targets.
add_compile_definitions(BOOST_ALL_NO_LIB)