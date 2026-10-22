# toolchains/freebsd/freebsd-llvm.cmake
#
# CMake toolchain for cross-compiling NZBGet for FreeBSD 13.0+ (amd64 / aarch64)
# using system/LLVM Clang with the FreeBSD 13.0 base sysroot and a static
# libc++/libc++abi/libunwind runtime built from LLVM sources.
#
# Usage:
#   cmake --preset ci-freebsd-x86_64
#   cmake --preset ci-freebsd-aarch64
#

set(CMAKE_SYSTEM_NAME "FreeBSD")
set(CMAKE_SYSTEM_VERSION "13.0")
set(FREEBSD TRUE)
set(NZBGET_BUILD_PLATFORM "freebsd" CACHE STRING "Target platform" FORCE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Propagate project-specific cross-compilation variables to try_compile subprojects
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
	FREEBSD
	FREEBSD_ARCH
	FREEBSD_SYSROOT
	FREEBSD_LIBCXX
)

# 1. Architecture configuration
if(NOT DEFINED FREEBSD_ARCH OR FREEBSD_ARCH STREQUAL "")
	if(DEFINED CMAKE_SYSTEM_PROCESSOR AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "")
		set(FREEBSD_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
	else()
		set(FREEBSD_ARCH "x86_64")
	endif()
endif()

if(FREEBSD_ARCH MATCHES "amd64|x86_64")
	set(CMAKE_SYSTEM_PROCESSOR "x86_64")
	set(FREEBSD_TARGET_TRIPLE "x86_64-pc-freebsd13")
	set(SYSROOT_DIR_NAME "sysroot")
	set(LIBCXX_DIR_NAME "libcxx")
	set(OPENSSL_TARGET_PLATFORM "BSD-x86_64" CACHE STRING "" FORCE)
elseif(FREEBSD_ARCH MATCHES "arm64|aarch64")
	set(CMAKE_SYSTEM_PROCESSOR "aarch64")
	set(FREEBSD_TARGET_TRIPLE "aarch64-pc-freebsd13")
	set(SYSROOT_DIR_NAME "sysroot-aarch64")
	set(LIBCXX_DIR_NAME "libcxx-aarch64")
	set(OPENSSL_TARGET_PLATFORM "BSD-aarch64" CACHE STRING "" FORCE)
else()
	message(FATAL_ERROR "Unsupported FREEBSD_ARCH: '${FREEBSD_ARCH}'. Must be x86_64/amd64 or aarch64/arm64.")
endif()

# 2. Sysroot detection
if(NOT DEFINED FREEBSD_SYSROOT OR FREEBSD_SYSROOT STREQUAL "")
	if(DEFINED CMAKE_SYSROOT AND NOT "${CMAKE_SYSROOT}" STREQUAL "")
		set(FREEBSD_SYSROOT "${CMAKE_SYSROOT}")
	elseif(DEFINED ENV{FREEBSD_PREFIX} AND NOT "$ENV{FREEBSD_PREFIX}" STREQUAL "")
		# Image layout is sysroot-<arch>. Older trees use sysroot / sysroot-aarch64.
		set(_FREEBSD_IMAGE_SYSROOT "$ENV{FREEBSD_PREFIX}/sysroot-${CMAKE_SYSTEM_PROCESSOR}")
		if(EXISTS "${_FREEBSD_IMAGE_SYSROOT}/usr/include")
			set(FREEBSD_SYSROOT "${_FREEBSD_IMAGE_SYSROOT}")
		else()
			get_filename_component(FREEBSD_SYSROOT "$ENV{FREEBSD_PREFIX}/${SYSROOT_DIR_NAME}" ABSOLUTE)
		endif()
	else()
		get_filename_component(FREEBSD_SYSROOT "${CMAKE_CURRENT_LIST_DIR}/../../build/toolchains/freebsd/${SYSROOT_DIR_NAME}" ABSOLUTE)
	endif()
endif()

if(NOT EXISTS "${FREEBSD_SYSROOT}/usr/include")
	message(FATAL_ERROR
		"FreeBSD sysroot not found in '${FREEBSD_SYSROOT}'. "
		"Run 'bash toolchains/freebsd/build-toolchain.sh ${FREEBSD_ARCH}' first.")
endif()

set(CMAKE_SYSROOT "${FREEBSD_SYSROOT}" CACHE PATH "Sysroot" FORCE)

# 3. Static C++ runtime detection (built by toolchains/freebsd/build-toolchain.sh)
if(NOT DEFINED FREEBSD_LIBCXX OR FREEBSD_LIBCXX STREQUAL "")
	get_filename_component(_FREEBSD_TOOLCHAIN_DIR "${FREEBSD_SYSROOT}" DIRECTORY)
	set(FREEBSD_LIBCXX "${_FREEBSD_TOOLCHAIN_DIR}/${LIBCXX_DIR_NAME}")
endif()

if(NOT EXISTS "${FREEBSD_LIBCXX}/include/c++/v1" OR NOT EXISTS "${FREEBSD_LIBCXX}/lib/libc++.a")
	message(FATAL_ERROR
		"Static libc++ runtime not found in '${FREEBSD_LIBCXX}'. "
		"Run 'bash toolchains/freebsd/build-toolchain.sh ${FREEBSD_ARCH}' first.")
endif()

set(FREEBSD_LIBCXX "${FREEBSD_LIBCXX}" CACHE PATH "Static libc++ runtime" FORCE)

# 4. Compilers detection
find_program(CLANG_C NAMES clang)
find_program(CLANG_CXX NAMES clang++)
find_program(LLD_LINKER NAMES ld.lld lld)
find_program(LLVM_AR NAMES llvm-ar)
find_program(LLVM_RANLIB NAMES llvm-ranlib)
find_program(LLVM_STRIP NAMES llvm-strip)

if(NOT CLANG_C OR NOT CLANG_CXX)
	message(FATAL_ERROR "Clang compiler not found. Please install clang.")
endif()
if(NOT LLD_LINKER)
	message(FATAL_ERROR "LLD linker not found. Please install lld.")
endif()

set(CMAKE_C_COMPILER "${CLANG_C}" CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${CLANG_CXX}" CACHE FILEPATH "C++ compiler" FORCE)
set(CMAKE_ASM_COMPILER "${CLANG_C}" CACHE FILEPATH "ASM compiler" FORCE)
set(CMAKE_LINKER "${LLD_LINKER}" CACHE FILEPATH "Linker" FORCE)
if(LLVM_AR)
	set(CMAKE_AR "${LLVM_AR}" CACHE FILEPATH "AR" FORCE)
endif()
if(LLVM_RANLIB)
	set(CMAKE_RANLIB "${LLVM_RANLIB}" CACHE FILEPATH "Ranlib" FORCE)
endif()
if(LLVM_STRIP)
	set(CMAKE_STRIP "${LLVM_STRIP}" CACHE FILEPATH "Strip" FORCE)
endif()

# 5. Target flags configuration
set(CMAKE_C_COMPILER_TARGET "${FREEBSD_TARGET_TRIPLE}")
set(CMAKE_CXX_COMPILER_TARGET "${FREEBSD_TARGET_TRIPLE}")
set(CMAKE_ASM_COMPILER_TARGET "${FREEBSD_TARGET_TRIPLE}")

set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "Disable C++20 module scanning" FORCE)

# Static libc++ headers/runtime built for the FreeBSD 13.0 target & static LLD linkage.
# -nostdinc++ keeps the ancient libc++ 11 headers of the base sysroot out of the build.
set(CMAKE_CXX_FLAGS_INIT "-nostdinc++ -isystem ${FREEBSD_LIBCXX}/include/c++/v1")
# libunwind.a is linked as a whole archive on purpose: FreeBSD's static libthr.a
# references _Unwind_ForcedUnwind and _Unwind_GetCFA as *weak* symbols, and a weak
# reference never extracts a lazy archive member. Without this, _Unwind_GetCFA
# resolves to NULL while _Unwind_ForcedUnwind does not, so every thread crashes on
# exit when libthr's thread_unwind_stop() calls the NULL pointer.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld -static -L${FREEBSD_LIBCXX}/lib -lc++ -lc++abi -Wl,--whole-archive ${FREEBSD_LIBCXX}/lib/libunwind.a -Wl,--no-whole-archive -lclang_rt.builtins")

if(DEFINED CMAKE_EXE_LINKER_FLAGS AND NOT CMAKE_EXE_LINKER_FLAGS MATCHES "-fuse-ld=")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld" CACHE STRING "Linker flags" FORCE)
endif()

# Prefer static libraries (.a) during dependency discovery on FreeBSD sysroot
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" CACHE STRING "Search only static archives on FreeBSD sysroot" FORCE)

# 6. Search path modes
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
