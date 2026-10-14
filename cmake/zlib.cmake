# zlib 1.3.1 - FetchContent build (pure CMake on all platforms)
# When BUILD_DEPS_FROM_SOURCE=OFF: tries system find_package first (local development)
# When BUILD_DEPS_FROM_SOURCE=ON: skips system lookup for hermetic builds

include(FetchContent)

set(ZLIB_VERSION "1.3.1")
set(ZLIB_SHA256 "9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23")
set(ZLIB_URL "https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}/zlib-${ZLIB_VERSION}.tar.gz")

if(NOT BUILD_DEPS_FROM_SOURCE)
	# Local development: try system zlib first
	find_package(ZLIB QUIET)
	if(ZLIB_FOUND)
		message(STATUS "Using system zlib ${ZLIB_VERSION} (found via find_package)")
		list(APPEND EXTERNAL_DEPS ZLIB::ZLIB)
		set(ZLIB_FROM_SYSTEM 1)
	endif()
endif()

if(NOT ZLIB_FROM_SYSTEM)
	message(STATUS "Building zlib ${ZLIB_VERSION} via FetchContent")

	# Disable system zlib lookup to force using FetchContent
	set(zlib_DIR "" CACHE PATH "" FORCE)

	# Configure zlib before populating: static build, no testing, no examples
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
	set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	# Explicitly disable shared library build in zlib's CMake
	set(zlib_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

	# EXCLUDE_FROM_ALL: keep zlib install rules out of the nzbget install,
	# while still generating deps/zlib-build/cmake_install.cmake so that the
	# parent install script can include it unconditionally.
	FetchContent_Declare(zlib
		URL ${ZLIB_URL}
		URL_HASH SHA256=${ZLIB_SHA256}
		EXCLUDE_FROM_ALL
	)

	FetchContent_MakeAvailable(zlib)

	if(MSVC AND TARGET zlibstatic)
		set_target_properties(zlibstatic PROPERTIES
			MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
		)
	endif()

	# Exclude shared zlib target from default build when building statically
	if(TARGET zlib)
		set_target_properties(zlib PROPERTIES EXCLUDE_FROM_ALL TRUE)
	endif()

	# zlib's CMake provides targets 'zlib' (shared) and 'zlibstatic' (static)
	# Since BUILD_SHARED_LIBS=OFF, zlibstatic is the static library target
	# Create standard aliases for compatibility
	if(NOT TARGET ZLIB::ZLIB)
		add_library(ZLIB::ZLIB ALIAS zlibstatic)
	endif()

	# Use the actual zlibstatic target for build dependencies
	list(APPEND EXTERNAL_DEPS zlibstatic)
endif()
