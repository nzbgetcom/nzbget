# toolchains/freebsd-llvm.cmake
#
# CMake toolchain for cross-compiling NZBGet for FreeBSD 13.0+ (amd64 / aarch64)
# using system/LLVM Clang 19 with FreeBSD base sysroot.
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

# Ensure CMake propagates architecture and sysroot variables to try_compile subprojects
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
	FREEBSD
	FREEBSD_ARCH
	FREEBSD_SYSROOT
	CMAKE_SYSROOT
	CMAKE_SYSTEM_NAME
	CMAKE_SYSTEM_PROCESSOR
	CMAKE_C_COMPILER_TARGET
	CMAKE_CXX_COMPILER_TARGET
)

# 1. Architecture configuration
if(NOT DEFINED FREEBSD_ARCH OR FREEBSD_ARCH STREQUAL "")
	if(DEFINED CMAKE_SYSTEM_PROCESSOR AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "")
		set(FREEBSD_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
	elseif(DEFINED ENV{FREEBSD_ARCH} AND NOT "$ENV{FREEBSD_ARCH}" STREQUAL "")
		set(FREEBSD_ARCH "$ENV{FREEBSD_ARCH}")
	elseif(DEFINED ENV{ARCH} AND NOT "$ENV{ARCH}" STREQUAL "")
		set(FREEBSD_ARCH "$ENV{ARCH}")
	else()
		set(FREEBSD_ARCH "x86_64")
	endif()
endif()

if(FREEBSD_ARCH STREQUAL "amd64" OR FREEBSD_ARCH STREQUAL "x86_64")
	set(CMAKE_SYSTEM_PROCESSOR "x86_64")
	set(FREEBSD_TARGET_TRIPLE "x86_64-pc-freebsd13")
	set(SYSROOT_DIR_NAME "sysroot")
	set(OPENSSL_TARGET_PLATFORM "BSD-x86_64" CACHE STRING "" FORCE)
elseif(FREEBSD_ARCH STREQUAL "arm64" OR FREEBSD_ARCH STREQUAL "aarch64")
	set(CMAKE_SYSTEM_PROCESSOR "aarch64")
	set(FREEBSD_TARGET_TRIPLE "aarch64-pc-freebsd13")
	set(SYSROOT_DIR_NAME "sysroot-aarch64")
	set(OPENSSL_TARGET_PLATFORM "BSD-aarch64" CACHE STRING "" FORCE)
else()
	message(FATAL_ERROR "Unsupported FREEBSD_ARCH: '${FREEBSD_ARCH}'. Must be x86_64/amd64 or aarch64/arm64.")
endif()

# 2. Sysroot detection
if(NOT DEFINED FREEBSD_SYSROOT OR FREEBSD_SYSROOT STREQUAL "")
	if(DEFINED CMAKE_SYSROOT AND NOT "${CMAKE_SYSROOT}" STREQUAL "")
		set(FREEBSD_SYSROOT "${CMAKE_SYSROOT}")
	elseif(DEFINED ENV{FREEBSD_SYSROOT} AND NOT "$ENV{FREEBSD_SYSROOT}" STREQUAL "")
		set(FREEBSD_SYSROOT "$ENV{FREEBSD_SYSROOT}")
	elseif(EXISTS "/build/freebsd/${SYSROOT_DIR_NAME}")
		set(FREEBSD_SYSROOT "/build/freebsd/${SYSROOT_DIR_NAME}")
	else()
		get_filename_component(FREEBSD_SYSROOT "${CMAKE_CURRENT_LIST_DIR}/../build/toolchains/freebsd/${SYSROOT_DIR_NAME}" ABSOLUTE)
	endif()
endif()

if(NOT EXISTS "${FREEBSD_SYSROOT}/usr/include")
	message(FATAL_ERROR
		"FreeBSD sysroot not found in '${FREEBSD_SYSROOT}'. "
		"Run 'bash toolchains/build-freebsd.sh ${FREEBSD_ARCH}' first.")
endif()

set(CMAKE_SYSROOT "${FREEBSD_SYSROOT}" CACHE PATH "Sysroot" FORCE)

# 3. Compilers detection (LLVM 19 preferred, fallback to clang)
find_program(CLANG_19_C NAMES clang-19 clang)
find_program(CLANG_19_CXX NAMES clang++-19 clang++)
find_program(LLD_19 NAMES ld.lld-19 ld.lld)
if(NOT LLD_19)
	find_program(LLD_19 NAMES lld-19 lld)
endif()
find_program(LLVM_AR NAMES llvm-ar-19 llvm-ar ar)
find_program(LLVM_RANLIB NAMES llvm-ranlib-19 llvm-ranlib ranlib)
find_program(LLVM_STRIP NAMES llvm-strip-19 llvm-strip strip)

if(NOT CLANG_19_C OR NOT CLANG_19_CXX)
	message(FATAL_ERROR "Clang compiler (clang-19) not found. Please install clang-19 and lld-19.")
endif()

set(CMAKE_C_COMPILER "${CLANG_19_C}" CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${CLANG_19_CXX}" CACHE FILEPATH "C++ compiler" FORCE)
set(CMAKE_ASM_COMPILER "${CLANG_19_C}" CACHE FILEPATH "ASM compiler" FORCE)
if(LLD_19)
	set(CMAKE_LINKER "${LLD_19}" CACHE FILEPATH "Linker" FORCE)
endif()
if(LLVM_AR)
	set(CMAKE_AR "${LLVM_AR}" CACHE FILEPATH "AR" FORCE)
endif()
if(LLVM_RANLIB)
	set(CMAKE_RANLIB "${LLVM_RANLIB}" CACHE FILEPATH "Ranlib" FORCE)
endif()
if(LLVM_STRIP)
	set(CMAKE_STRIP "${LLVM_STRIP}" CACHE FILEPATH "Strip" FORCE)
endif()

# 4. Target flags configuration
set(CMAKE_C_COMPILER_TARGET "${FREEBSD_TARGET_TRIPLE}")
set(CMAKE_CXX_COMPILER_TARGET "${FREEBSD_TARGET_TRIPLE}")
set(CMAKE_ASM_COMPILER_TARGET "${FREEBSD_TARGET_TRIPLE}")

set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "Disable C++20 module scanning" FORCE)

set(_COMMON_FLAGS "--target=${FREEBSD_TARGET_TRIPLE} --sysroot=${FREEBSD_SYSROOT} -isystem ${FREEBSD_SYSROOT}/usr/include/c++/v1")
set(CMAKE_C_FLAGS_INIT "${_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${_COMMON_FLAGS}")
if(NOT CMAKE_C_FLAGS MATCHES "--target=")
	set(CMAKE_C_FLAGS "${_COMMON_FLAGS} ${CMAKE_C_FLAGS}")
endif()
if(NOT CMAKE_CXX_FLAGS MATCHES "--target=")
	set(CMAKE_CXX_FLAGS "${_COMMON_FLAGS} ${CMAKE_CXX_FLAGS}")
endif()

if(LLD_19)
	set(_LLD_FLAG "-fuse-ld=${LLD_19}")
else()
	set(_LLD_FLAG "-fuse-ld=lld")
endif()

set(_LINKER_FLAGS "--target=${FREEBSD_TARGET_TRIPLE} --sysroot=${FREEBSD_SYSROOT} ${_LLD_FLAG} -static")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_LINKER_FLAGS}")
if(NOT CMAKE_EXE_LINKER_FLAGS MATCHES "--target=")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_LINKER_FLAGS}")
endif()

# Prefer static libraries (.a) during dependency discovery on FreeBSD sysroot
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" CACHE STRING "Search only static archives on FreeBSD sysroot" FORCE)

# 5. Search path modes
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
