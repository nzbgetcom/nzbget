cmake_minimum_required(VERSION 3.13)

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed. You should create separate directory for build files.")
endif()

set_property(GLOBAL PROPERTY PACKAGE)
set_property(GLOBAL PROPERTY LIBS)
set_property(GLOBAL PROPERTY INCLUDES)

set(VERSION "24.2")
set(PACKAGE "nzbget")
set(PACKAGE_BUGREPORT "https://github.com/nzbgetcom/nzbget/issues")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CONFIGURATION_TYPES "Release" "Debug")

string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" VERSION_MATCH ${VERSION})
set(VERSION_MAJOR ${CMAKE_MATCH_1})
set(VERSION_MINOR ${CMAKE_MATCH_2})

add_compile_definitions(HAVE_CONFIG_H=1)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
endif()

option(BUILD_ONLY_TESTS "Build only tests (for CI)")

project(
	nzbget
	VERSION ${VERSION}
	DESCRIPTION "NZBGet is a binary downloader, which downloads files from Usenet"
	LANGUAGES C CXX
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
		set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g0 -pthread -g -DDEBUG -Weverything -Wno-c++98-compat" CACHE STRING "" FORCE)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_CXX_FLAGS "-O0 -g0 -pthread -g -DDEBUG -Wall -Wextra" CACHE STRING "" FORCE)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		set(CMAKE_CXX_FLAGS "/Od /Zi /MTd /MP /W4 /EHs /DDEBUG /D_DEBUG /DWIN32 /wd4800 /wd4267" CACHE STRING "" FORCE)
		set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} winmm.lib Dbghelp.lib libcpmtd.lib" CACHE STRING "" FORCE)
	endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
		set(CMAKE_CXX_FLAGS "-O2 -g0 -pthread -DNDEBUG -Weverything -Wno-c++98-compat" CACHE STRING "" FORCE)
		set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-s" CACHE STRING "" FORCE)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_CXX_FLAGS "-O2 -g0 -pthread -DNDEBUG -Wall -Wextra" CACHE STRING "" FORCE)
		set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-s" CACHE STRING "" FORCE)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		set(CMAKE_CXX_FLAGS "/O2 /MT /MP /W4 /EHs /DNDEBUG /DWIN32 /wd4800 /wd4267" CACHE STRING "" FORCE)
		set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} winmm.lib" CACHE STRING "" FORCE)
	endif()
endif()

set(CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS} CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS} CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS} CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS} CACHE STRING "" FORCE)

include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})

if(NOT BUILD_ONLY_TESTS)
	include(daemon/sources.cmake)
	add_executable(${PACKAGE} ${SRC})
endif()

if(WIN32)
	include(cmake/windows.cmake)
	if(NOT BUILD_ONLY_TESTS)
		target_sources(${PACKAGE} PRIVATE ${CMAKE_SOURCE_DIR}/windows/resources/nzbget.rc)
		configure_file(
			${CMAKE_SOURCE_DIR}/windows/resources/version.rc.in
			${CMAKE_BINARY_DIR}/version.rc
		)
		configure_file(
			${CMAKE_SOURCE_DIR}/windows/version.nsi.in
			${CMAKE_BINARY_DIR}/version.nsi
			@ONLY
		)
		configure_file(
			${CMAKE_SOURCE_DIR}/windows/version-uninstall.nsi.in
			${CMAKE_BINARY_DIR}/version-uninstall.nsi
			@ONLY
		)
		target_sources(${PACKAGE} PRIVATE ${CMAKE_BINARY_DIR}/version.rc)
	endif()
else()
	include(cmake/posix.cmake)
	if(NOT BUILD_ONLY_TESTS)
		include(${CMAKE_SOURCE_DIR}/cmake/install.cmake)
	endif()
endif()

configure_file(
	${CMAKE_SOURCE_DIR}/cmake/config.h.in
	${CMAKE_BINARY_DIR}/config.h
)

if(NOT BUILD_ONLY_TESTS)
	target_link_libraries(${PACKAGE} PRIVATE ${LIBS})
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
		${INCLUDES}
	)
endif()

if(ENABLE_TESTS OR BUILD_ONLY_TESTS)
	include(CTest)
	add_subdirectory(tests)
endif()
