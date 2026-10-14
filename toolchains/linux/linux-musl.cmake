# toolchains/linux-musl.cmake
#
# Universal hermetic CMake toolchain for cross-compiling NZBGet for Linux (musl libc)
# across 7 architectures: x86_64, aarch64, armhf, armel, i686, ppc6xx, riscv64.
#
# Toolchains prepared by toolchains/linux/build-toolchain.sh (musl-cross-make with GCC 14.3.0 + musl 1.2.6).
#
# Usage:
#   cmake --preset ci-linux-x86_64
#   cmake --preset ci-linux-aarch64
#   cmake --preset ci-linux-armhf
#   cmake --preset ci-linux-armel
#   cmake --preset ci-linux-i686
#   cmake --preset ci-linux-ppc6xx
#   cmake --preset ci-linux-riscv64
#

set(CMAKE_SYSTEM_NAME Linux)
set(NZBGET_BUILD_PLATFORM "linux" CACHE STRING "Target platform" FORCE)

# Ensure CMake propagates architecture and sysroot variables to try_compile subprojects
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
	LINUX_ARCH
	LINUX_TARGET_TRIPLE
	LINUX_ARCH_FLAGS
	NZBGET_LINUX_TOOLCHAIN_DIR
	TOOLCHAIN_PREFIX
	OPENSSL_TARGET_PLATFORM
	CMAKE_SYSROOT
	CMAKE_SYSTEM_NAME
	CMAKE_SYSTEM_PROCESSOR
)

# 1. Architecture detection & configuration
if(NOT DEFINED LINUX_ARCH OR LINUX_ARCH STREQUAL "")
	if(DEFINED CMAKE_SYSTEM_PROCESSOR AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "")
		set(LINUX_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
	elseif(DEFINED ENV{LINUX_ARCH} AND NOT "$ENV{LINUX_ARCH}" STREQUAL "")
		set(LINUX_ARCH "$ENV{LINUX_ARCH}")
	elseif(DEFINED ENV{ARCH} AND NOT "$ENV{ARCH}" STREQUAL "")
		set(LINUX_ARCH "$ENV{ARCH}")
	elseif(DEFINED NZBGET_TARGET AND NOT "${NZBGET_TARGET}" STREQUAL "")
		set(LINUX_ARCH "${NZBGET_TARGET}")
	else()
		set(LINUX_ARCH "x86_64")
	endif()
endif()

set(_ARCH_FLAGS "")
set(_STACK_CLASH_PROTECTION FALSE)

if(LINUX_ARCH STREQUAL "x86_64" OR LINUX_ARCH STREQUAL "amd64" OR LINUX_ARCH STREQUAL "x86_64-linux-musl")
	set(LINUX_ARCH "x86_64")
	set(CMAKE_SYSTEM_PROCESSOR "x86_64")
	set(LINUX_TARGET_TRIPLE "x86_64-linux-musl")
	set(OPENSSL_TARGET_PLATFORM "linux-x86_64" CACHE STRING "" FORCE)
	set(_ARCH_FLAGS "-march=x86-64 -mtune=generic -fcf-protection=full")
	set(_STACK_CLASH_PROTECTION TRUE)
elseif(LINUX_ARCH STREQUAL "aarch64" OR LINUX_ARCH STREQUAL "arm64" OR LINUX_ARCH STREQUAL "aarch64-linux-musl")
	set(LINUX_ARCH "aarch64")
	set(CMAKE_SYSTEM_PROCESSOR "aarch64")
	set(LINUX_TARGET_TRIPLE "aarch64-linux-musl")
	set(OPENSSL_TARGET_PLATFORM "linux-aarch64" CACHE STRING "" FORCE)
	set(_ARCH_FLAGS "-march=armv8-a -mbranch-protection=standard")
	set(_STACK_CLASH_PROTECTION TRUE)
elseif(LINUX_ARCH STREQUAL "armhf" OR LINUX_ARCH STREQUAL "arm-linux-musleabihf")
	set(LINUX_ARCH "armhf")
	set(CMAKE_SYSTEM_PROCESSOR "arm")
	set(LINUX_TARGET_TRIPLE "arm-linux-musleabihf")
	set(OPENSSL_TARGET_PLATFORM "linux-armv4" CACHE STRING "" FORCE)
	# Debian armhf baseline: VFPv3-D16 only, Thumb-2, Hard-Float (prevents SIGILL on non-NEON Cortex-A9/Tegra 2)
	set(_ARCH_FLAGS "-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -mthumb")
elseif(LINUX_ARCH STREQUAL "armel" OR LINUX_ARCH STREQUAL "arm-linux-musleabi")
	set(LINUX_ARCH "armel")
	set(CMAKE_SYSTEM_PROCESSOR "arm")
	set(LINUX_TARGET_TRIPLE "arm-linux-musleabi")
	set(OPENSSL_TARGET_PLATFORM "linux-armv4" CACHE STRING "" FORCE)
	# Debian armel baseline: ARMv5TE soft-float (prevents SIGILL on ARM926EJ-S / Kirkwood / Orion NAS)
	set(_ARCH_FLAGS "-march=armv5te -mfloat-abi=soft")
elseif(LINUX_ARCH STREQUAL "i686" OR LINUX_ARCH STREQUAL "x86" OR LINUX_ARCH STREQUAL "i686-linux-musl")
	set(LINUX_ARCH "i686")
	set(CMAKE_SYSTEM_PROCESSOR "i686")
	set(LINUX_TARGET_TRIPLE "i686-linux-musl")
	set(OPENSSL_TARGET_PLATFORM "linux-x86" CACHE STRING "" FORCE)
	# Debian i386 baseline: generic Pentium Pro / i686 class without mandatory SSE2
	set(_ARCH_FLAGS "-march=i686 -mtune=generic")
	set(_STACK_CLASH_PROTECTION TRUE)
elseif(LINUX_ARCH STREQUAL "ppc6xx" OR LINUX_ARCH STREQUAL "powerpc" OR LINUX_ARCH STREQUAL "powerpc-linux-musl")
	set(LINUX_ARCH "ppc6xx")
	set(CMAKE_SYSTEM_PROCESSOR "powerpc")
	set(LINUX_TARGET_TRIPLE "powerpc-linux-musl")
	set(OPENSSL_TARGET_PLATFORM "linux-ppc" CACHE STRING "" FORCE)
	# Debian powerpc baseline: 32-bit PowerPC without mandatory AltiVec (prevents SIGILL on e300/603e NAS)
	set(_ARCH_FLAGS "-mcpu=powerpc")
elseif(LINUX_ARCH STREQUAL "riscv64" OR LINUX_ARCH STREQUAL "riscv64-linux-musl")
	set(LINUX_ARCH "riscv64")
	set(CMAKE_SYSTEM_PROCESSOR "riscv64")
	set(LINUX_TARGET_TRIPLE "riscv64-linux-musl")
	set(OPENSSL_TARGET_PLATFORM "linux64-riscv64" CACHE STRING "" FORCE)
	# Debian riscv64 baseline: RV64GC with lp64d ABI
	set(_ARCH_FLAGS "-march=rv64gc -mabi=lp64d")
else()
	message(FATAL_ERROR "Unsupported LINUX_ARCH: '${LINUX_ARCH}'. Must be x86_64, aarch64, armhf, armel, i686, ppc6xx, or riscv64.")
endif()

set(LINUX_ARCH_FLAGS "${_ARCH_FLAGS}" CACHE STRING "Target architecture flags for Linux musl" FORCE)

# 2. Toolchain directory and prefix resolution
if(NOT DEFINED NZBGET_LINUX_TOOLCHAIN_DIR OR NZBGET_LINUX_TOOLCHAIN_DIR STREQUAL "")
	if(DEFINED ENV{NZBGET_LINUX_TOOLCHAIN_DIR} AND NOT "$ENV{NZBGET_LINUX_TOOLCHAIN_DIR}" STREQUAL "")
		set(NZBGET_LINUX_TOOLCHAIN_DIR "$ENV{NZBGET_LINUX_TOOLCHAIN_DIR}")
	else()
		get_filename_component(NZBGET_LINUX_TOOLCHAIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../../build/toolchains/linux" ABSOLUTE)
	endif()
endif()

set(_TOOLCHAIN_BIN_CANDIDATES
	"${NZBGET_LINUX_TOOLCHAIN_DIR}/${LINUX_ARCH}/bin/${LINUX_TARGET_TRIPLE}"
	"${NZBGET_LINUX_TOOLCHAIN_DIR}/${LINUX_TARGET_TRIPLE}/bin/${LINUX_TARGET_TRIPLE}"
	"/opt/cross/${LINUX_TARGET_TRIPLE}/bin/${LINUX_TARGET_TRIPLE}"
	"/opt/toolchains/${LINUX_TARGET_TRIPLE}/bin/${LINUX_TARGET_TRIPLE}"
)

set(_NZBGET_PREFIX "")
if(DEFINED TOOLCHAIN_PREFIX AND NOT "${TOOLCHAIN_PREFIX}" STREQUAL "")
	set(_NZBGET_PREFIX "${TOOLCHAIN_PREFIX}")
else()
	foreach(_CAND ${_TOOLCHAIN_BIN_CANDIDATES})
		if(EXISTS "${_CAND}-gcc")
			set(_NZBGET_PREFIX "${_CAND}")
			break()
		endif()
	endforeach()
endif()

if(NOT _NZBGET_PREFIX)
	# Fallback: check if compiler is directly available in PATH
	find_program(_PATH_GCC "${LINUX_TARGET_TRIPLE}-gcc")
	if(_PATH_GCC)
		string(REGEX REPLACE "-gcc$" "" _NZBGET_PREFIX "${_PATH_GCC}")
	else()
		message(FATAL_ERROR
			"Musl cross toolchain not found for target '${LINUX_TARGET_TRIPLE}'.\n"
			"Checked candidate paths:\n"
			"  ${NZBGET_LINUX_TOOLCHAIN_DIR}/${LINUX_ARCH}/bin/${LINUX_TARGET_TRIPLE}-gcc\n"
			"  ${NZBGET_LINUX_TOOLCHAIN_DIR}/${LINUX_TARGET_TRIPLE}/bin/${LINUX_TARGET_TRIPLE}-gcc\n"
			"  /opt/cross/${LINUX_TARGET_TRIPLE}/bin/${LINUX_TARGET_TRIPLE}-gcc\n"
			"Please run 'bash toolchains/linux/build-toolchain.sh ${LINUX_ARCH}' first.")
	endif()
endif()

set(CMAKE_C_COMPILER    "${_NZBGET_PREFIX}-gcc"     CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER  "${_NZBGET_PREFIX}-g++"     CACHE FILEPATH "C++ compiler" FORCE)
set(CMAKE_ASM_COMPILER  "${_NZBGET_PREFIX}-gcc"     CACHE FILEPATH "ASM compiler" FORCE)
set(CMAKE_LINKER        "${_NZBGET_PREFIX}-ld"      CACHE FILEPATH "Linker" FORCE)
set(CMAKE_OBJCOPY       "${_NZBGET_PREFIX}-objcopy" CACHE FILEPATH "Objcopy" FORCE)

if(EXISTS "${_NZBGET_PREFIX}-gcc-ar")
	set(CMAKE_AR "${_NZBGET_PREFIX}-gcc-ar" CACHE FILEPATH "AR" FORCE)
else()
	set(CMAKE_AR "${_NZBGET_PREFIX}-ar" CACHE FILEPATH "AR" FORCE)
endif()

if(EXISTS "${_NZBGET_PREFIX}-gcc-ranlib")
	set(CMAKE_RANLIB "${_NZBGET_PREFIX}-gcc-ranlib" CACHE FILEPATH "Ranlib" FORCE)
else()
	set(CMAKE_RANLIB "${_NZBGET_PREFIX}-ranlib" CACHE FILEPATH "Ranlib" FORCE)
endif()
set(CMAKE_SIZE          "${_NZBGET_PREFIX}-size"    CACHE FILEPATH "Size" FORCE)
set(CMAKE_STRIP         "${_NZBGET_PREFIX}-strip"   CACHE FILEPATH "Strip" FORCE)

# 3. Sysroot resolution
set(_SYSROOT_CANDIDATES
	"${_NZBGET_PREFIX}/../../${LINUX_TARGET_TRIPLE}/sysroot"
	"${_NZBGET_PREFIX}/../../${LINUX_TARGET_TRIPLE}"
	"${_NZBGET_PREFIX}/../../staging"
)

if(DEFINED NZBGET_SYSROOT AND EXISTS "${NZBGET_SYSROOT}")
	set(CMAKE_SYSROOT "${NZBGET_SYSROOT}" CACHE PATH "Sysroot" FORCE)
else()
	foreach(_SYS ${_SYSROOT_CANDIDATES})
		get_filename_component(_SYS_ABS "${_SYS}" ABSOLUTE)
		if(EXISTS "${_SYS_ABS}/usr/include" OR EXISTS "${_SYS_ABS}/include")
			set(CMAKE_SYSROOT "${_SYS_ABS}" CACHE PATH "Sysroot" FORCE)
			break()
		endif()
	endforeach()
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 4. Target compilation and security hardening flags
set(_COMMON_C_CXX_FLAGS "${LINUX_ARCH_FLAGS} -fstack-protector-strong")
if(_STACK_CLASH_PROTECTION)
	string(APPEND _COMMON_C_CXX_FLAGS " -fstack-clash-protection")
endif()

set(CMAKE_C_FLAGS_INIT   "${_COMMON_C_CXX_FLAGS} -fvisibility=hidden")
set(CMAKE_CXX_FLAGS_INIT "${_COMMON_C_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-z,relro,-z,now -Wl,-z,noexecstack -static")

# 5. Disable CMake's C++20 module dependency scanning (avoids clang-scan-deps lookup on cross-compilation)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "Disable C++20 module scanning" FORCE)

message(STATUS "NZBGet Linux Musl Toolchain: Target=${LINUX_TARGET_TRIPLE}, Arch=${LINUX_ARCH}, Prefix=${_NZBGET_PREFIX}")
