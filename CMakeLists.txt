cmake_minimum_required(VERSION 3.22)

project(
    nzbget
    VERSION 22.0
    DESCRIPTION "NZBGet is a binary downloader, which downloads files from Usenet"
    LANGUAGES C CXX
)

option(ENABLE_TESTING "Enable the tests" ON)

add_compile_definitions(VERSION="22" PACKAGE="nzbget")

configure_file(config.h.in config.h)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF)
set(OPENSSL_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Weverything")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /W4")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /NODEFAULTLIB:msvcrt.lib;libcmt.lib;msvcrtd.lib")
endif()

if (WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(VCPKG_TARGET_TRIPLET "x64-windows-static")
    else()
        set(VCPKG_TARGET_TRIPLET "x86-windows-static")
    endif()
endif()

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(lib/regex)

add_library(Yencode STATIC
    ${CMAKE_SOURCE_DIR}/lib/yencode/SimdInit.cpp
    ${CMAKE_SOURCE_DIR}/lib/yencode/SimdDecoder.cpp
    ${CMAKE_SOURCE_DIR}/lib/yencode/ScalarDecoder.cpp
    ${CMAKE_SOURCE_DIR}/lib/yencode/Sse2Decoder.cpp
    ${CMAKE_SOURCE_DIR}/lib/yencode/Ssse3Decoder.cpp
    ${CMAKE_SOURCE_DIR}/lib/yencode/PclmulCrc.cpp
    ${CMAKE_SOURCE_DIR}/lib/yencode/NeonDecoder.cpp
    ${CMAKE_SOURCE_DIR}/lib/yencode/AcleCrc.cpp
    ${CMAKE_SOURCE_DIR}/lib/yencode/SliceCrc.cpp
)
target_include_directories(Yencode PUBLIC 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/lib/yencode 
    ${CMAKE_SOURCE_DIR}/daemon/main
    ${ZLIB_INCLUDE_DIR}
	${OPENSSL_INCLUDE_DIR}
)

add_library(Par2 STATIC
    ${CMAKE_SOURCE_DIR}/lib/par2/commandline.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/crc.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/creatorpacket.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/criticalpacket.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/datablock.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/descriptionpacket.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/diskfile.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/filechecksummer.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/galois.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/mainpacket.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/md5.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/par2fileformat.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/par2repairer.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/par2repairersourcefile.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/parheaders.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/recoverypacket.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/reedsolomon.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/verificationhashtable.cpp
    ${CMAKE_SOURCE_DIR}/lib/par2/verificationpacket.cpp
)
target_include_directories(Par2 PUBLIC 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/lib/par2 
    ${CMAKE_SOURCE_DIR}/daemon/main
    ${CMAKE_SOURCE_DIR}/daemon/util
    ${OPENSSL_INCLUDE_DIR}
)

if(ENABLE_TESTING)
    include(CTest)
    add_subdirectory(tests)
endif()