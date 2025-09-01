cmake_minimum_required(VERSION 3.13)

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed. You should create separate directory for build files.")
endif()

set(VERSION "25.3")
set(PACKAGE "nzbget")
set(LIBS "")
set(INCLUDES ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

set(PACKAGE_BUGREPORT "https://github.com/nzbgetcom/nzbget/issues")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)

string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" VERSION_MATCH ${VERSION})
set(VERSION_MAJOR ${CMAKE_MATCH_1})
set(VERSION_MINOR ${CMAKE_MATCH_2})

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

option(BUILD_ONLY_TESTS "Build only tests (for CI)")

project(
	nzbget
	VERSION ${VERSION}
	DESCRIPTION "NZBGet is a binary downloader, which downloads files from Usenet"
	LANGUAGES C CXX
)

include(cmake/common.cmake)
include(daemon/sources.cmake)

if(NOT BUILD_ONLY_TESTS)
	add_executable(${PACKAGE} ${SRC})
	target_precompile_headers(${PACKAGE} PRIVATE ${CMAKE_SOURCE_DIR}/daemon/main/nzbget.h)

	if(NOT WIN32)
		if(CMAKE_BUILD_TYPE STREQUAL "Release")
			add_custom_command(
				TARGET ${PACKAGE} POST_BUILD
				DEPENDS ${PACKAGE}
				COMMAND ${CMAKE_STRIP}
				ARGS ${PACKAGE}
			)
		endif()
	endif()
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
	target_include_directories(${PACKAGE} PRIVATE ${INCLUDES})
endif()

if(ENABLE_TESTS OR BUILD_ONLY_TESTS)
	include(CTest)
	add_subdirectory(tests)
endif()
