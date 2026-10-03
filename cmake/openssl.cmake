# OpenSSL 3.5.9 LTS - FetchContent build via jimmy-park/openssl-cmake
# Provides standard OpenSSL::SSL and OpenSSL::Crypto targets
# When BUILD_DEPS_FROM_SOURCE=OFF: tries system find_package first (local development)
# When BUILD_DEPS_FROM_SOURCE=ON: skips system lookup entirely for hermetic builds

include(FetchContent)

set(OPENSSL_VERSION "3.5.9")

if(BUILD_DEPS_FROM_SOURCE)
    message(STATUS "Building OpenSSL ${OPENSSL_VERSION} via jimmy-park/openssl-cmake FetchContent")
else()
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        message(STATUS "Using system OpenSSL (found via find_package)")
        set(OPENSSL_FROM_SYSTEM 1)
    else()
        message(STATUS "System OpenSSL not found, building OpenSSL ${OPENSSL_VERSION} via jimmy-park/openssl-cmake FetchContent")
    endif()
endif()

if(NOT OPENSSL_FROM_SYSTEM)
    # On Windows, building OpenSSL from source requires Perl
    if(WIN32)
        find_program(PERL_EXECUTABLE perl)
        if(NOT PERL_EXECUTABLE)
            message(FATAL_ERROR
                "Building OpenSSL from source on Windows requires Perl.\n"
                "Please install Strawberry Perl (e.g. 'winget install StrawberryPerl.StrawberryPerl' or 'choco install strawberryperl')\n"
                "or configure with -DDISABLE_TLS=ON if TLS is not needed."
            )
        endif()

        # Explicitly set OpenSSL target platform to avoid auto-detecting host AMD64 for 32-bit builds
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(OPENSSL_TARGET_PLATFORM "VC-WIN64A" CACHE STRING "OpenSSL target platform" FORCE)
        else()
            set(OPENSSL_TARGET_PLATFORM "VC-WIN32" CACHE STRING "OpenSSL target platform" FORCE)
        endif()
        message(STATUS "OpenSSL Windows target platform: ${OPENSSL_TARGET_PLATFORM}")
    endif()

    # Disable OpenSSL features we don't need (same as our previous config)
    # no-engine is deprecated in OpenSSL 3.5+
    set(OPENSSL_CONFIGURE_OPTIONS
        no-shared
        no-tests
        no-apps
        no-dso
        no-comp
        no-dgram
        no-ct
        no-ocsp
        no-cms
        no-ts
        no-srp
        no-srtp
        no-aria
        no-sm2
        no-sm3
        no-sm4
        no-gost
        no-idea
        no-bf
        no-cast
        no-rc2
        no-rc4
        no-seed
        no-whirlpool
        no-rmd160
        no-mdc2
        no-md4
        no-blake2
        no-rfc3779
    )

    # Pin OpenSSL version to 3.5.9 LTS
    set(OPENSSL_TARGET_VERSION ${OPENSSL_VERSION} CACHE STRING "OpenSSL version to build" FORCE)

    # Ensure static runtime on MSVC (/MT or /MTd) - inherited from parent CMake
    # openssl-cmake respects CMAKE_MSVC_RUNTIME_LIBRARY

    # Disable ccache in openssl-cmake to avoid crashes with MSVC / non-UTF8 paths on Windows
    set(OPENSSL_USE_CCACHE OFF CACHE BOOL "Use ccache if available" FORCE)

    FetchContent_Declare(
        openssl_cmake
        GIT_REPOSITORY https://github.com/jimmy-park/openssl-cmake.git
        GIT_TAG main
    )

    FetchContent_MakeAvailable(openssl_cmake)

    # openssl-cmake automatically provides OpenSSL::SSL and OpenSSL::Crypto targets
    set(OPENSSL_FROM_SYSTEM 0)
endif()

# Both paths provide OpenSSL::SSL and OpenSSL::Crypto targets
# Export variable for include directories if needed by consumers
if(OPENSSL_FROM_SYSTEM)
    # System OpenSSL - include dirs already in target properties
    set(OPENSSL_INCLUDE_DIR "")
else()
    # openssl-cmake - include dirs available via target
    set(OPENSSL_INCLUDE_DIR "")
endif()

# OpenSSL is either found in system or built via FetchContent
# In both cases targets OpenSSL::SSL and OpenSSL::Crypto are available
list(APPEND EXTERNAL_DEPS OpenSSL::SSL OpenSSL::Crypto)