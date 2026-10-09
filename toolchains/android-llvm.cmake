# toolchains/android-llvm.cmake
#
# CMake toolchain for cross-compiling NZBGet for Android API 21+ using upstream
# LLVM Clang 19.1.7 with compiler-rt builtins, isolated static libc++/libc++abi/libunwind
# and Bionic sysroot. Prepare everything with: bash toolchains/setup-android-toolchain.sh
#
# Architecture options (pass via -DANDROID_ARCH=<arch> or CMakePresets.json):
#   arm64   -> aarch64-linux-android21 (arm64-v8a)
#   armv7   -> armv7a-linux-androideabi21 (armeabi-v7a)
#   x86_64  -> x86_64-linux-android21 (x86_64)
#   x86     -> i686-linux-android21 (x86)
#

set(CMAKE_SYSTEM_NAME Linux)
set(ANDROID TRUE)
set(NZBGET_BUILD_PLATFORM "android" CACHE STRING "Target platform" FORCE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Ensure CMake propagates architecture and sysroot variables to try_compile subprojects
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
	ANDROID
	ANDROID_ARCH
	NZBGET_ANDROID_TOOLCHAIN_DIR
	ANDROID_API_LEVEL
	ANDROID_TARGET_FULL
	ANDROID_SYSROOT
	CMAKE_SYSROOT
	CMAKE_C_COMPILER_TARGET
	CMAKE_CXX_COMPILER_TARGET
)

# 1. Resolve Toolchain directory
if(NOT DEFINED NZBGET_ANDROID_TOOLCHAIN_DIR OR NZBGET_ANDROID_TOOLCHAIN_DIR STREQUAL "")
	if(DEFINED ENV{NZBGET_ANDROID_TOOLCHAIN_DIR} AND NOT "$ENV{NZBGET_ANDROID_TOOLCHAIN_DIR}" STREQUAL "")
		set(NZBGET_ANDROID_TOOLCHAIN_DIR "$ENV{NZBGET_ANDROID_TOOLCHAIN_DIR}")
	else()
		get_filename_component(NZBGET_ANDROID_TOOLCHAIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../build/toolchains" ABSOLUTE)
	endif()
endif()

# 2. Architecture & API level configuration (Synchronized with toolchains/setup-android-toolchain.sh)
set(_API_LEVEL_FILE "${NZBGET_ANDROID_TOOLCHAIN_DIR}/android-libcxx/android-api-level")
if(NOT EXISTS "${_API_LEVEL_FILE}")
	set(_API_LEVEL_FILE "${NZBGET_ANDROID_TOOLCHAIN_DIR}/android-api-level")
endif()
set(_TOOLCHAIN_API_LEVEL "")
if(EXISTS "${_API_LEVEL_FILE}")
	file(READ "${_API_LEVEL_FILE}" _TOOLCHAIN_API_LEVEL)
	string(STRIP "${_TOOLCHAIN_API_LEVEL}" _TOOLCHAIN_API_LEVEL)
endif()

if(NOT DEFINED ANDROID_API_LEVEL OR ANDROID_API_LEVEL STREQUAL "")
	if(DEFINED ENV{ANDROID_API_LEVEL} AND NOT "$ENV{ANDROID_API_LEVEL}" STREQUAL "")
		set(ANDROID_API_LEVEL "$ENV{ANDROID_API_LEVEL}" CACHE STRING "Minimum Android API Level" FORCE)
	elseif(_TOOLCHAIN_API_LEVEL)
		set(ANDROID_API_LEVEL "${_TOOLCHAIN_API_LEVEL}" CACHE STRING "Minimum Android API Level" FORCE)
	else()
		set(ANDROID_API_LEVEL "21" CACHE STRING "Minimum Android API Level")
	endif()
endif()

if(_TOOLCHAIN_API_LEVEL AND NOT "${ANDROID_API_LEVEL}" STREQUAL "${_TOOLCHAIN_API_LEVEL}")
	message(FATAL_ERROR
		"Configured ANDROID_API_LEVEL (${ANDROID_API_LEVEL}) does not match the toolchain runtime API level "
		"(${_TOOLCHAIN_API_LEVEL}) recorded in ${_API_LEVEL_FILE}.\n"
		"The static libc++/compiler-rt runtimes were compiled targeting API ${_TOOLCHAIN_API_LEVEL}.\n"
		"To re-target the toolchain runtimes, run:\n"
		"  ANDROID_API_LEVEL=${ANDROID_API_LEVEL} bash toolchains/setup-android-toolchain.sh\n"
		"Or configure CMake with matching API level: -DANDROID_API_LEVEL=${_TOOLCHAIN_API_LEVEL}")
endif()

if(NOT DEFINED ANDROID_ARCH OR ANDROID_ARCH STREQUAL "")
	set(ANDROID_ARCH "arm64" CACHE STRING "Target Android architecture (arm64, armv7, x86_64, x86)")
endif()

set(_ARCH_FLAGS "")
if(ANDROID_ARCH STREQUAL "arm64")
	set(CMAKE_SYSTEM_PROCESSOR "aarch64")
	set(ANDROID_TARGET_TRIPLE "aarch64-linux-android")
	set(OPENSSL_TARGET_PLATFORM "linux-aarch64" CACHE STRING "" FORCE)
elseif(ANDROID_ARCH STREQUAL "armv7")
	set(CMAKE_SYSTEM_PROCESSOR "arm")
	set(ANDROID_TARGET_TRIPLE "armv7a-linux-androideabi")
	set(_ARCH_FLAGS "-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb")
	set(OPENSSL_TARGET_PLATFORM "linux-armv4" CACHE STRING "" FORCE)
elseif(ANDROID_ARCH STREQUAL "x86_64")
	set(CMAKE_SYSTEM_PROCESSOR "x86_64")
	set(ANDROID_TARGET_TRIPLE "x86_64-linux-android")
	set(OPENSSL_TARGET_PLATFORM "linux-x86_64-clang" CACHE STRING "" FORCE)
elseif(ANDROID_ARCH STREQUAL "x86")
	set(CMAKE_SYSTEM_PROCESSOR "i686")
	set(ANDROID_TARGET_TRIPLE "i686-linux-android")
	set(_ARCH_FLAGS "-march=i686")
	set(OPENSSL_TARGET_PLATFORM "linux-x86-clang" CACHE STRING "" FORCE)
else()
	message(FATAL_ERROR "Unsupported ANDROID_ARCH: '${ANDROID_ARCH}'. Must be arm64, armv7, x86_64, or x86.")
endif()

set(ANDROID_ARCH_FLAGS "${_ARCH_FLAGS}" CACHE STRING "Target architecture flags for Android" FORCE)

set(ANDROID_TARGET_FULL "${ANDROID_TARGET_TRIPLE}${ANDROID_API_LEVEL}")

# 3. Hermetic LLVM Clang/LLD 19.1.7 prepared by toolchains/setup-android-toolchain.sh

set(_LLVM_BIN "${NZBGET_ANDROID_TOOLCHAIN_DIR}/android-llvm/bin")
if(NOT EXISTS "${_LLVM_BIN}/clang" OR NOT EXISTS "${_LLVM_BIN}/ld.lld")
	message(FATAL_ERROR
		"Android LLVM toolchain not found in ${_LLVM_BIN}. "
		"Run 'bash toolchains/setup-android-toolchain.sh' first.")
endif()

set(CMAKE_C_COMPILER "${_LLVM_BIN}/clang" CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${_LLVM_BIN}/clang++" CACHE FILEPATH "C++ compiler" FORCE)
set(CMAKE_ASM_COMPILER "${_LLVM_BIN}/clang" CACHE FILEPATH "ASM compiler" FORCE)
set(CMAKE_AR "${_LLVM_BIN}/llvm-ar" CACHE FILEPATH "AR" FORCE)
set(CMAKE_RANLIB "${_LLVM_BIN}/llvm-ranlib" CACHE FILEPATH "Ranlib" FORCE)
set(CMAKE_STRIP "${_LLVM_BIN}/llvm-strip" CACHE FILEPATH "Strip" FORCE)
set(CMAKE_LINKER "${_LLVM_BIN}/ld.lld" CACHE FILEPATH "Linker" FORCE)

# 3. Bionic API 21 sysroot
set(ANDROID_SYSROOT "${NZBGET_ANDROID_TOOLCHAIN_DIR}/android-sysroot")
if(NOT EXISTS "${ANDROID_SYSROOT}/usr/include")
	message(FATAL_ERROR
		"Android sysroot not found in ${ANDROID_SYSROOT}. "
		"Run 'bash toolchains/setup-android-toolchain.sh' first.")
endif()
set(CMAKE_SYSROOT "${ANDROID_SYSROOT}" CACHE PATH "Sysroot" FORCE)
message(STATUS "NZBGet: Android LLVM -> ${_LLVM_BIN}, sysroot -> ${ANDROID_SYSROOT}")

# 4. Target flags configuration
set(CMAKE_C_COMPILER_TARGET "${ANDROID_TARGET_FULL}")
set(CMAKE_CXX_COMPILER_TARGET "${ANDROID_TARGET_FULL}")
set(CMAKE_ASM_COMPILER_TARGET "${ANDROID_TARGET_FULL}")

# Disable CMake's C++20 module dependency scanning (P1689/clang-scan-deps) for cross-compilation
# CMake 3.28+ automatically scans for C++20 modules when CMAKE_CXX_STANDARD >= 20,
# which invokes clang-scan-deps. NZBGet does not use C++ modules.
# This avoids "CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS-NOTFOUND" errors.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "Disable C++20 module scanning" FORCE)

set(_COMMON_C_CXX_FLAGS "--target=${ANDROID_TARGET_FULL} --sysroot=${ANDROID_SYSROOT} -D__ANDROID__ -fPIC -fPIE -fstack-protector-strong -D_FORTIFY_SOURCE=2 ${_ARCH_FLAGS}")
set(CMAKE_C_FLAGS_INIT "${_COMMON_C_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${_COMMON_C_CXX_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${_COMMON_C_CXX_FLAGS}")
if(NOT CMAKE_C_FLAGS MATCHES "--target=")
	set(CMAKE_C_FLAGS "${_COMMON_C_CXX_FLAGS} ${CMAKE_C_FLAGS}")
endif()
if(NOT CMAKE_CXX_FLAGS MATCHES "--target=")
	set(CMAKE_CXX_FLAGS "${_COMMON_C_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")
endif()
if(NOT CMAKE_ASM_FLAGS MATCHES "--target=")
	set(CMAKE_ASM_FLAGS "${_COMMON_C_CXX_FLAGS} ${CMAKE_ASM_FLAGS}")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Flags used by the C compiler" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "Flags used by the C++ compiler" FORCE)
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS}" CACHE STRING "Flags used by the ASM compiler" FORCE)

# 5. Isolated static libc++ runtime
set(NZBGET_STATIC_LIBCXX "${NZBGET_ANDROID_TOOLCHAIN_DIR}/android-libcxx")
set(_LIBCXX_ARCH_LIB "${NZBGET_STATIC_LIBCXX}/${ANDROID_ARCH}/lib")
if(NOT EXISTS "${_LIBCXX_ARCH_LIB}/libc++.a" OR NOT IS_DIRECTORY "${NZBGET_STATIC_LIBCXX}/include/c++/v1")
	message(FATAL_ERROR
		"Static Android libc++ not found for '${ANDROID_ARCH}' in ${NZBGET_STATIC_LIBCXX}. "
		"Run 'bash toolchains/setup-android-toolchain.sh ${ANDROID_ARCH}' first.")
endif()

if(NOT CMAKE_CXX_FLAGS MATCHES "-nostdinc\\+\\+")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdinc++ -isystem ${NZBGET_STATIC_LIBCXX}/include/c++/v1 -D_LIBCPP_DISABLE_AVAILABILITY")
endif()

# compiler-rt builtins replace libgcc; libunwind comes from the static libc++ build
set(_STATIC_LIBS "-Wl,--whole-archive ${_LIBCXX_ARCH_LIB}/libc++.a -Wl,--no-whole-archive ${_LIBCXX_ARCH_LIB}/libc++abi.a ${_LIBCXX_ARCH_LIB}/libunwind.a")
set(_LINK_FLAGS "--target=${ANDROID_TARGET_FULL} --sysroot=${ANDROID_SYSROOT} ${_ARCH_FLAGS} -fuse-ld=lld -rtlib=compiler-rt --unwindlib=none -Wl,--no-dependent-libraries -pie -nostdlib++ -Wl,-z,relro,-z,now -Wl,-z,noexecstack ${_STATIC_LIBS} -ldl -lm -llog")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_LINK_FLAGS}")
if(NOT CMAKE_EXE_LINKER_FLAGS MATCHES "nostdlib\\+\\+")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_LINK_FLAGS}")
endif()

# 6. Bionic pthread settings (threads are part of Bionic libc.so)
set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "" FORCE)
set(CMAKE_HAVE_THREADS_LIBRARY 1 CACHE BOOL "" FORCE)
set(CMAKE_USE_WIN32_THREADS_INIT 0 CACHE BOOL "" FORCE)
set(CMAKE_USE_PTHREADS_INIT 1 CACHE BOOL "" FORCE)
set(THREADS_PREFER_PTHREAD_FLAG ON CACHE BOOL "" FORCE)
set(CMAKE_HAVE_PTHREADS_CREATE 0 CACHE BOOL "" FORCE)
set(Threads_FOUND TRUE CACHE BOOL "" FORCE)

# 7. Root search paths for cross-compilation
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 8. Force standard compiler-based dependency scanning (-MD -MF) instead of
# experimental clang-scan-deps (requires clang-tools package not installed in CI).
set(CMAKE_DEPENDS_USE_COMPILER TRUE CACHE BOOL "Use compiler for dependency scanning" FORCE)
