# libxml2 2.13.5 - Pure CMake FetchContent
# Static, no python, no icu, no lzma, no programs
# When BUILD_DEPS_FROM_SOURCE=OFF: tries system find_package first (local development)
# When BUILD_DEPS_FROM_SOURCE=ON: skips system lookup for hermetic builds

include(FetchContent)

if(NOT BUILD_DEPS_FROM_SOURCE)
    # Local development: try system libxml2 first
    find_package(LibXml2 QUIET)
    if(LibXml2_FOUND)
        message(STATUS "Using system libxml2 (found via find_package)")
        list(APPEND EXTERNAL_DEPS LibXml2::LibXml2)
        set(LIBXML2_FROM_SYSTEM 1)
    endif()
endif()

if(NOT LIBXML2_FROM_SYSTEM)
    message(STATUS "Building libxml2 2.13.5 via FetchContent")

    # Configure libxml2 options before populating
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(LIBXML2_WITH_ICONV OFF CACHE BOOL "" FORCE)
    set(LIBXML2_WITH_LZMA OFF CACHE BOOL "" FORCE)
    set(LIBXML2_WITH_PYTHON OFF CACHE BOOL "" FORCE)
    set(LIBXML2_WITH_ZLIB ON CACHE BOOL "" FORCE)
    set(LIBXML2_WITH_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(LIBXML2_WITH_TESTS OFF CACHE BOOL "" FORCE)

    # Disable dld/shl_load check on macOS (dld library doesn't exist on macOS)
    if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        set(HAVE_SHLLOAD OFF CACHE BOOL "" FORCE)
    endif()

    # Disable system libxml2 lookup to force using FetchContent
    set(LibXml2_DIR "" CACHE PATH "" FORCE)

    FetchContent_Declare(
        libxml2
        URL "https://gitlab.gnome.org/GNOME/libxml2/-/archive/v2.13.5/libxml2-v2.13.5.tar.gz"
        URL_HASH SHA256=37cdec8cd20af8ab0decfa2419b09b4337c2dbe9da5615d2a26f547449fecf2a
    )

    set(_PREV_SKIP_INSTALL_RULES ${CMAKE_SKIP_INSTALL_RULES})
    set(CMAKE_SKIP_INSTALL_RULES ON)
    FetchContent_MakeAvailable(libxml2)
    set(CMAKE_SKIP_INSTALL_RULES ${_PREV_SKIP_INSTALL_RULES})

    # Disable CRT deprecation warnings (e.g. strncpy) and strictness errors under MSVC
    if(MSVC AND TARGET LibXml2)
        target_compile_definitions(LibXml2 PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(LibXml2 PRIVATE /sdl- /wd4701 /wd4703)
        set_target_properties(LibXml2 PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()

    # libxml2's CMake provides target 'LibXml2' (static when BUILD_SHARED_LIBS=OFF)
    # CMake 3.31+ automatically provides imported target 'LibXml2::LibXml2'
    # Use the target that actually exists (system libxml2 provides LibXml2::LibXml2)
    list(APPEND EXTERNAL_DEPS LibXml2::LibXml2)
endif()