cmake_minimum_required(VERSION 3.22)
if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(VERSION "23.0")
set(PACKAGE "nzbget")
set(PACKAGE_BUGREPORT "https://github.com/nzbgetcom/nzbget/issues")

project(
    nzbget
    VERSION ${VERSION}
    DESCRIPTION "NZBGet is a binary downloader, which downloads files from Usenet"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug or Release)" FORCE)
endif()

project(
	${PACKAGE}
	VERSION ${VERSION}
	DESCRIPTION "NZBGet is a binary downloader, which downloads files from Usenet."
	LANGUAGES C CXX
)

option(ENABLE_TESTS "Enable tests")
option(BUILD_SHARED_LIBS "Build shared libs")
option(DISABLE_TLS "Disable TLS")
option(DISABLE_LIBXML2 "Disable libxml2")
option(DISABLE_CURSES "Disable curses")
option(DISABLE_GZIP "Disable gzip")
option(DISABLE_PARCHECK "Disable parcheck")
option(DISABLE_SIGCHLD_HANDLER "Do not use sigchld-handler")
if(NOT DISABLE_TLS)
	option(USE_OPENSSL "Use OpenSSL" ON)
	option(USE_GNUTLS "Use GnuTLS" OFF)
endif()

if(NOT BUILD_SHARED_LIBS)
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a" ".lib")
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

message(STATUS "Options:")
message(STATUS "  BUILD_TYPE:              ${CMAKE_BUILD_TYPE}")
message(STATUS "  ENABLE_TESTS:            ${ENABLE_TESTS}")
message(STATUS "  DISABLE_TLS:             ${DISABLE_TLS}")
message(STATUS "  - OPENSSL:               ${USE_OPENSSL}")
message(STATUS "  - GNUTLS:                ${USE_GNUTLS}")
message(STATUS "  DISABLE_CURSES:          ${DISABLE_CURSES}")
message(STATUS "  DISABLE_GZIP:            ${DISABLE_GZIP}")
message(STATUS "  DISABLE_PARCHECK:        ${DISABLE_PARCHECK}")
message(STATUS "  DISABLE_SIGCHLD_HANDLER: ${DISABLE_SIGCHLD_HANDLER}")

if(NOT DISABLE_GZIP)
	find_package(ZLIB REQUIRED)
endif()

if(USE_OPENSSL)
	find_package(OpenSSL REQUIRED)
endif()

if(USE_GNUTLS)
	find_package(GnuTLS REQUIRED)
endif()

if(NOT DISABLE_CURSES)
	find_package(Curses REQUIRED)
endif()

find_package(LibXml2 REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)

include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})

include(cmake/config.cmake)
configure_file(
	${CMAKE_SOURCE_DIR}/cmake/config.h.in
	${CMAKE_BINARY_DIR}/config.h
)

add_subdirectory(daemon)
add_subdirectory(lib)

add_executable(${PACKAGE} ${SRC})
target_include_directories(${PACKAGE} PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/connect
	${CMAKE_SOURCE_DIR}/daemon/extension
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/frontend
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/daemon/nserv
	${CMAKE_SOURCE_DIR}/daemon/postprocess
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/lib/par2
	${CMAKE_SOURCE_DIR}/lib/regex
	${CMAKE_SOURCE_DIR}/lib/yencode
	${Boost_INCLUDE_DIR}
	${LIBXML2_INCLUDE_DIR}
	${CURSES_INCLUDE_DIR}
)
if(WIN32)
	target_include_directories(${PACKAGE} PRIVATE
		${CMAKE_SOURCE_DIR}/daemon/windows
		${CMAKE_SOURCE_DIR}/windows/resources
	)
	target_sources(${PACKAGE} PRIVATE ${CMAKE_SOURCE_DIR}/windows/resources/nzbget.rc)
endif()

target_link_libraries(${PACKAGE} PRIVATE
	Yencode
	Regex
	OpenSSL::SSL
	OpenSSL::Crypto
	ZLIB::ZLIB
	Boost::json
	${LIBXML2_LIBRARIES}
	${CURSES_LIBRARIES}
)

if(NOT DISABLE_PARCHECK)
	target_link_libraries(${PACKAGE} PRIVATE Par2)
endif()

if(ENABLE_TESTS)
	include(CTest)
	add_subdirectory(tests)
endif()
