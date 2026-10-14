include(FetchContent)

if(NOT BUILD_DEPS_FROM_SOURCE)
	# Local development: try system libxml2 first
	find_package(LibXml2 QUIET)
	if(LibXml2_FOUND)
		message(STATUS "Using system libxml2 (found via find_package)")
		list(APPEND EXTERNAL_DEPS LibXml2::LibXml2)
		set(LIBXML2_FROM_SYSTEM 1)
	endif()
endif()

if(NOT LIBXML2_FROM_SYSTEM)
	message(STATUS "Building libxml2 2.13.5 via FetchContent")

	# Configure libxml2 options before populating
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_ICONV OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_LZMA OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_PYTHON OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_ZLIB OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_PROGRAMS OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_TESTS OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_MODULES OFF CACHE BOOL "" FORCE)

	# Disable unneeded features to minimize footprint and compile time
	set(LIBXML2_WITH_C14N OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_CATALOG OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_DEBUG OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_HTML OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_SCHEMAS OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_SCHEMATRON OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_XPATH OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_XPTR OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_XINCLUDE OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_WRITER OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_PATTERN OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_READER OFF CACHE BOOL "" FORCE)
	set(LIBXML2_WITH_REGEXPS OFF CACHE BOOL "" FORCE)

	# Disable dld/shl_load check
	set(HAVE_SHLLOAD OFF CACHE BOOL "" FORCE)

	# Disable getentropy: musl getentropy() uses sys_getrandom which returns ENOSYS (error 38)
	# on Linux kernels < 3.17, causing libxml2 to abort() with core dump at startup on NAS devices.
	# Android API < 28 also lacks header declaration. Disabling forces portable PRNG fallback.
	set(HAVE_GETENTROPY OFF CACHE BOOL "" FORCE)

	# Disable system libxml2 lookup to force using FetchContent
	set(LibXml2_DIR "" CACHE PATH "" FORCE)

	# EXCLUDE_FROM_ALL: keep libxml2 install rules out of the nzbget install,
	# while still generating deps/libxml2-build/cmake_install.cmake so that the
	# parent install script can include it unconditionally.
	FetchContent_Declare(
		libxml2
		URL "https://gitlab.gnome.org/GNOME/libxml2/-/archive/v2.13.5/libxml2-v2.13.5.tar.gz"
		URL_HASH SHA256=37cdec8cd20af8ab0decfa2419b09b4337c2dbe9da5615d2a26f547449fecf2a
		EXCLUDE_FROM_ALL
	)

	# Enable C extensions (GNU extensions, e.g. -std=gnu17) for libxml2
	# libxml2 relies on POSIX/Linux extensions (getentropy, fileno, etc.)
	# which strict ISO C (-std=c17 from root CMAKE_C_EXTENSIONS=OFF) hides in musl/glibc.
	set(_PREV_CMAKE_C_EXTENSIONS ${CMAKE_C_EXTENSIONS})
	set(CMAKE_C_EXTENSIONS ON)

	FetchContent_MakeAvailable(libxml2)

	set(CMAKE_C_EXTENSIONS ${_PREV_CMAKE_C_EXTENSIONS})

	if(TARGET LibXml2)
		set_target_properties(LibXml2 PROPERTIES C_EXTENSIONS ON)
	endif()

	# Disable CRT deprecation warnings (e.g. strncpy) and strictness errors under MSVC
	if(MSVC AND TARGET LibXml2)
		target_compile_definitions(LibXml2 PRIVATE _CRT_SECURE_NO_WARNINGS)
		target_compile_options(LibXml2 PRIVATE /sdl- /wd4701 /wd4703)
		set_target_properties(LibXml2 PROPERTIES
			MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
		)
	endif()

	# libxml2's CMake provides target 'LibXml2' (static when BUILD_SHARED_LIBS=OFF)
	# Ensure LibXml2::LibXml2 imported target / alias exists
	if(TARGET LibXml2 AND NOT TARGET LibXml2::LibXml2)
		add_library(LibXml2::LibXml2 ALIAS LibXml2)
	endif()
	list(APPEND EXTERNAL_DEPS LibXml2::LibXml2)
endif()
