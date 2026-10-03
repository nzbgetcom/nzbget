cmake_minimum_required(VERSION 3.13)

if (POLICY CMP0167)
	cmake_policy(SET CMP0167 NEW)
endif()

if (POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
endif()

if (POLICY CMP0091)
	cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

# Skip all install() rules from FetchContent dependencies on Windows
# NZBGet Windows installers are packaged via custom NSIS scripts
if(WIN32)
	set(CMAKE_SKIP_INSTALL_RULES ON)
endif()

# Force all FetchContent downloads/builds into a fixed, cacheable directory
# This ensures CI cache (build/deps) captures all dependencies
set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/deps CACHE PATH "FetchContent base directory" FORCE)

# Option to build external dependencies from source via FetchContent
# ON  = CI / reproducible builds (always build from pinned sources, no system lookup)
# OFF = Local development (try find_package first, fallback to FetchContent)
option(BUILD_DEPS_FROM_SOURCE "Build external dependencies from source via FetchContent (for CI / reproducible builds)" OFF)

# Backwards compatibility aliases
if(BUILD_FROM_SOURCE OR FORCE_FETCH_DEPENDENCIES)
	set(BUILD_DEPS_FROM_SOURCE ON CACHE BOOL "" FORCE)
endif()

# Prepend local cmake/ directory to module path for in-tree FindZLIB.cmake
# Required so libxml2's find_package(ZLIB) finds the FetchContent zlibstatic target
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed. You should create separate directory for build files.")
endif()

set(VERSION "27.0")
set(PACKAGE "nzbget")
set(LIBS "")
set(INCLUDES ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

set(PACKAGE_BUGREPORT "https://github.com/nzbgetcom/nzbget/issues")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC runtime library: static (/MT for Release/MinSizeRel/RelWithDebInfo, /MTd for Debug)
# Controlled by CMP0091 (NEW). Set before project() to initialize all targets.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library" FORCE)

string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" VERSION_MATCH ${VERSION})
set(VERSION_MAJOR ${CMAKE_MATCH_1})
set(VERSION_MINOR ${CMAKE_MATCH_2})

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

option(BUILD_ONLY_TESTS "Build only tests (for CI)")
option(ENABLE_TESTS "Enable unit tests")
set(USE_SANITIZERS "" CACHE STRING
    "Comma-separated sanitizers: address,undefined,thread,memory,leak (MSVC supports only address)")

project(
	nzbget
	VERSION ${VERSION}
	DESCRIPTION "NZBGet is a binary downloader, which downloads files from Usenet"
	LANGUAGES C CXX
)

set(EXTERNAL_DEPS "")
include(cmake/common.cmake)
include(daemon/sources.cmake)

# Create the main executable target early so platform modules can link to it
if(NOT BUILD_ONLY_TESTS)
	add_executable(${PACKAGE} ${CMAKE_SOURCE_DIR}/daemon/main/nzbget.cpp ${SRC})

	if(WIN32)
		include(cmake/windows.cmake)
	else()
		include(cmake/posix.cmake)
	endif()

	# rapidyenc is needed for PCH (nzbget.h includes rapidyenc.h)
	# Ensure its include directory is available for PCH compilation
	if(TARGET rapidyenc::rapidyenc)
		target_include_directories(${PACKAGE} PRIVATE $<TARGET_PROPERTY:rapidyenc::rapidyenc,INTERFACE_INCLUDE_DIRECTORIES>)
	endif()

	target_precompile_headers(${PACKAGE} PRIVATE ${CMAKE_SOURCE_DIR}/daemon/main/nzbget.h)

	if(EXTERNAL_DEPS)
		add_dependencies(${PACKAGE} ${EXTERNAL_DEPS})
	endif()

	target_link_libraries(${PACKAGE} PRIVATE ${LIBS})
	target_include_directories(${PACKAGE} PRIVATE ${INCLUDES})

	apply_compiler_flags(${PACKAGE})
	apply_sanitizers(${PACKAGE})
else()
	# For BUILD_ONLY_TESTS, still need platform deps for test targets
	if(WIN32)
		include(cmake/windows.cmake)
	else()
		include(cmake/posix.cmake)
	endif()
endif()

configure_file(
	${CMAKE_SOURCE_DIR}/cmake/config.h.in
	${CMAKE_BINARY_DIR}/config.h
)

if(WIN32)
	if(NOT BUILD_ONLY_TESTS)
		target_sources(${PACKAGE} PRIVATE ${CMAKE_SOURCE_DIR}/platforms/windows/resources/nzbget.rc)
		configure_file(
			${CMAKE_SOURCE_DIR}/platforms/windows/resources/version.rc.in
			${CMAKE_BINARY_DIR}/version.rc
		)
		configure_file(
			${CMAKE_SOURCE_DIR}/platforms/windows/version.nsi.in
			${CMAKE_BINARY_DIR}/version.nsi
			@ONLY
		)
		configure_file(
			${CMAKE_SOURCE_DIR}/platforms/windows/version-uninstall.nsi.in
			${CMAKE_BINARY_DIR}/version-uninstall.nsi
			@ONLY
		)
		target_sources(${PACKAGE} PRIVATE ${CMAKE_BINARY_DIR}/version.rc)
	endif()
else()
	if(NOT BUILD_ONLY_TESTS)
		if(CMAKE_BUILD_TYPE STREQUAL "Release")
			add_custom_command(
				TARGET ${PACKAGE} POST_BUILD
				COMMAND ${CMAKE_STRIP}
				ARGS ${PACKAGE}
			)
		endif()
		include(${CMAKE_SOURCE_DIR}/cmake/install.cmake)
	endif()
endif()

if(ENABLE_TESTS OR BUILD_ONLY_TESTS)
	include(CTest)
	add_subdirectory(tests)
endif()
