cmake_minimum_required(VERSION 3.22)

set(VERSION "23")

project(
    nzbget
    VERSION ${VERSION}
    DESCRIPTION "NZBGet is a binary downloader, which downloads files from Usenet"
    LANGUAGES C CXX
)

option(ENABLE_TESTS "Enable tests" ON)

set(PACKAGE "nzbget")
add_compile_definitions(HAVE_CONFIG_H=1)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake_config.h.in
    ${CMAKE_BINARY_DIR}/config.h
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF)
set(OPENSSL_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Weverything")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /W4")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} winmm.lib /NODEFAULTLIB:msvcrt.lib;libcmt.lib;msvcrtd.lib")
endif()

include_directories(lib/regex)
include_directories(${CMAKE_BINARY_DIR})
add_subdirectory(lib)

if(ENABLE_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
