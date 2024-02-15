cmake_minimum_required(VERSION 3.22)

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed. You should create separate directory for build files.")
endif()

set_property(GLOBAL PROPERTY PACKAGE)

set(VERSION "23.0")
set(PACKAGE "nzbget")
set(PACKAGE_BUGREPORT "https://github.com/nzbgetcom/nzbget/issues")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CONFIGURATION_TYPES "Release" "Debug")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
endif()

project(
    nzbget
    VERSION ${VERSION}
    DESCRIPTION "NZBGet is a binary downloader, which downloads files from Usenet"
    LANGUAGES C CXX
)

include(cmake/common.cmake)
if(WIN32)
	include(cmake/windows.cmake)
else()
	include(cmake/posix.cmake)
endif()
configure_file(
	${CMAKE_SOURCE_DIR}/cmake/config.h.in
	${CMAKE_BINARY_DIR}/config.h
)

if(ENABLE_TESTS)
	include(CTest)
	add_subdirectory(tests)
endif()
