cmake_minimum_required(VERSION 3.22)

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(VERSION "22.2")
set(PACKAGE "nzbget")
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

option(ENABLE_SHARED "Enable shared" ON)
option(ENABLE_TESTS "Enable tests" ON)

message(STATUS "Options:")
message(STATUS "  BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "  ENABLE_SHARED: ${ENABLE_SHARED} (Use shared linking)")
message(STATUS "  ENABLE_TESTS: ${ENABLE_TESTS} (Build tests)")

set(TEST_SOURCE "${CMAKE_BINARY_DIR}/test.cpp")
file(WRITE ${TEST_SOURCE}
    "#include <time.h>\n"
    "int main() {\n"
    "  char buffer[26];\n"
    "  time_t currentTime;\n"
    "  ctime_r(&currentTime, buffer, 26);\n"
    "  return 0;\n"
    "}")

# Try compiling and linking the test program
try_compile(HAVE_CTIME_R_3 ${CMAKE_BINARY_DIR}/ctime_r_test
    SOURCES ${TEST_SOURCE}
)

if(HAVE_CTIME_R_3)
	set(HAVE_CTIME_R_3 1)
else()
	set(HAVE_CTIME_R_2 1)
endif()

# configure_file(
# 	${CMAKE_SOURCE_DIR}/config.h.in
# 	${CMAKE_BINARY_DIR}/config.h
# )

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -Weverything")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -Wall -Wextra")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Od /W4")
        set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} winmm.lib /NODEFAULTLIB:msvcrt.lib;libcmt.lib;msvcrtd.lib" CACHE STRING "")
    endif()
elseif(CMAKE_BUILD_TYPE MATCHES "Release")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Weverything")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /W4")
        set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} winmm.lib /NODEFAULTLIB:msvcrt.lib;libcmt.lib;msvcrtd.lib" CACHE STRING "")
    endif()
endif()

add_subdirectory(daemon)

set(BUILD_SHARED_LIBS OFF)
set(OPENSSL_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibXml2 REQUIRED)

if (NOT WIN32)
	find_package(Curses REQUIRED)
endif()

include_directories(lib/regex)
include_directories(${CMAKE_BINARY_DIR})
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
if (WIN32)
	target_include_directories(${PACKAGE} PRIVATE
		${CMAKE_SOURCE_DIR}/daemon/windows
		${CMAKE_SOURCE_DIR}/windows/resources
	)
	target_sources(${PACKAGE} PRIVATE ${CMAKE_SOURCE_DIR}/windows/resources/nzbget.rc)
endif()

target_link_libraries(${PACKAGE} PRIVATE
	Yencode
	Par2
	Regex
	OpenSSL::SSL
	OpenSSL::Crypto
	ZLIB::ZLIB
	LibXml2::LibXml2
	${CURSES_LIBRARIES}
)
target_precompile_headers(${PACKAGE} PRIVATE ${CMAKE_SOURCE_DIR}/daemon/main/nzbget.h)

if(ENABLE_TESTS)
	include(CTest)
	add_subdirectory(tests)
endif()
