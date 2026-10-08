# Allow FetchContent_Populate for non-CMake dependencies (CMake 3.30+)
if(POLICY CMP0169)
	cmake_policy(SET CMP0169 OLD)
endif()

include(FetchContent)
include(ExternalProject)

set(OPENSSL_VERSION "3.5.9")
set(OPENSSL_SHA256 "603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a")
set(OPENSSL_URL "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz")

if(NOT BUILD_DEPS_FROM_SOURCE)
	# Local development: try system OpenSSL first
	find_package(OpenSSL QUIET)
	if(OpenSSL_FOUND)
		message(STATUS "Using system OpenSSL (found via find_package)")
		list(APPEND EXTERNAL_DEPS OpenSSL::SSL OpenSSL::Crypto)
		return()
	endif()
endif()

# Version-tagged directory paths prevent stale binary reuse across version bumps (both locally and in CI)
set(OPENSSL_INSTALL_DIR "${CMAKE_BINARY_DIR}/deps/openssl-${OPENSSL_VERSION}")
set(OPENSSL_BUILD_DIR   "${CMAKE_BINARY_DIR}/deps/openssl-${OPENSSL_VERSION}-build")

if(WIN32)
	set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_INSTALL_DIR}/lib/libcrypto.lib")
	set(OPENSSL_SSL_LIBRARY    "${OPENSSL_INSTALL_DIR}/lib/libssl.lib")
else()
	set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_INSTALL_DIR}/lib/libcrypto.a")
	set(OPENSSL_SSL_LIBRARY    "${OPENSSL_INSTALL_DIR}/lib/libssl.a")
endif()

# Reuse prebuilt OpenSSL artifacts if already available in the dependency cache
if(EXISTS "${OPENSSL_CRYPTO_LIBRARY}" AND EXISTS "${OPENSSL_SSL_LIBRARY}" AND EXISTS "${OPENSSL_INSTALL_DIR}/include/openssl/ssl.h")
	message(STATUS "Using prebuilt OpenSSL ${OPENSSL_VERSION} from cache: ${OPENSSL_INSTALL_DIR}")
else()
	message(STATUS "Building OpenSSL ${OPENSSL_VERSION} via FetchContent")

	find_package(Perl QUIET)
	if(NOT PERL_EXECUTABLE)
		find_program(PERL_EXECUTABLE perl)
	endif()
	if(NOT PERL_EXECUTABLE)
		message(FATAL_ERROR
			"Building OpenSSL from source requires Perl.\n"
			"Please install Perl (e.g. Strawberry Perl on Windows: 'winget install StrawberryPerl.StrawberryPerl' or 'choco install strawberryperl')\n"
			"or configure with -DDISABLE_TLS=ON if TLS is not needed."
		)
	endif()

	FetchContent_Declare(openssl_src
		URL ${OPENSSL_URL}
		URL_HASH SHA256=${OPENSSL_SHA256}
	)
	FetchContent_GetProperties(openssl_src)
	if(NOT openssl_src_POPULATED)
		FetchContent_Populate(openssl_src)
	endif()

	# Pre-create install directories so CMake configure-time checks succeed
	file(MAKE_DIRECTORY "${OPENSSL_INSTALL_DIR}/include")
	file(MAKE_DIRECTORY "${OPENSSL_INSTALL_DIR}/lib")

	# Core options for minimal static TLS/Crypto libraries (no unused protocols/features)
	set(OPENSSL_CONFIGURE_OPTIONS
		no-shared
		threads
		no-tests
		no-apps
		no-docs
		no-legacy
		no-module
		no-engine
		no-quic
		no-dso
		no-comp
		no-dgram
		no-sctp
		no-ct
		no-ocsp
		no-cms
		no-ts
		no-srp
		no-srtp
		no-cmp
		no-rfc3779
		no-ui-console
		# Disabled unused/legacy symmetric ciphers
		no-aria
		no-bf
		no-camellia
		no-cast
		no-des
		no-idea
		no-rc2
		no-rc4
		no-seed
		no-sm4
		# Disabled unused/legacy hashes and MACs
		no-blake2
		no-md4
		no-mdc2
		no-rmd160
		no-siphash
		no-sm3
		no-whirlpool
		# Disabled unused asymmetric algorithms and curve types
		no-dsa
		no-ec2m
		no-sm2
		# Disabled post-quantum algorithms (OpenSSL 3.5+ large footprint)
		no-ml-dsa
		no-ml-kem
		no-slh-dsa
		# Disabled unused modes and KDFs
		no-siv
		no-scrypt
		no-ocb
		# Security: do not auto-load openssl.cnf from hardcoded build paths
		no-autoload-config
		# Unused protocols, engines and diagnostic features
		no-dtls
		no-ssl-trace
		no-psk
		no-nextprotoneg
		no-gost
		no-weak-ssl-ciphers
		no-capieng
		no-loadereng
		no-padlockeng
	)

	# Determine target architecture and build command
	if(WIN32)
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(OPENSSL_TARGET_PLATFORM "VC-WIN64A")
		else()
			set(OPENSSL_TARGET_PLATFORM "VC-WIN32")
		endif()

		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			list(APPEND OPENSSL_CONFIGURE_OPTIONS "/MTd" "--debug")
		else()
			list(APPEND OPENSSL_CONFIGURE_OPTIONS "/MT" "--release" "/Gy" "/Gw")
			if(ENABLE_LTO)
				if(CMAKE_C_COMPILE_OPTIONS_IPO)
					list(APPEND OPENSSL_CONFIGURE_OPTIONS ${CMAKE_C_COMPILE_OPTIONS_IPO})
				else()
					list(APPEND OPENSSL_CONFIGURE_OPTIONS "/GL")
				endif()
			endif()
		endif()

		find_program(NASM_EXECUTABLE nasm)
		if(NOT NASM_EXECUTABLE)
			message(WARNING "NASM assembler not found! OpenSSL will be built with no-asm (hardware crypto acceleration disabled). Install NASM to enable fast assembly implementations.")
			list(APPEND OPENSSL_CONFIGURE_OPTIONS "no-asm")
		endif()

		set(OPENSSL_BUILD_CMD nmake build_sw)
		set(OPENSSL_INSTALL_CMD nmake install_sw)
	else()
		if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
			list(APPEND OPENSSL_CONFIGURE_OPTIONS "-ffunction-sections" "-fdata-sections")
			if(ENABLE_LTO)
				list(APPEND OPENSSL_CONFIGURE_OPTIONS "-flto")
			endif()
		endif()

		# Target platform mapping for cross-compilation and POSIX
		if(NOT OPENSSL_TARGET_PLATFORM)
			if(NZBGET_BUILD_PLATFORM STREQUAL "android" OR ANDROID)
				if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
					set(OPENSSL_TARGET_PLATFORM "linux-aarch64")
				elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
					set(OPENSSL_TARGET_PLATFORM "linux-x86_64-clang")
				elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")
					set(OPENSSL_TARGET_PLATFORM "linux-x86-clang")
				else()
					set(OPENSSL_TARGET_PLATFORM "linux-armv4")
				endif()
			elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
				if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
					set(OPENSSL_TARGET_PLATFORM "BSD-aarch64")
				else()
					set(OPENSSL_TARGET_PLATFORM "BSD-x86_64")
				endif()
			elseif(APPLE)
				if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR (NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64"))
					set(OPENSSL_TARGET_PLATFORM "darwin64-arm64-cc")
				else()
					set(OPENSSL_TARGET_PLATFORM "darwin64-x86_64-cc")
				endif()
			elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
				if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
					set(OPENSSL_TARGET_PLATFORM "linux-x86_64")
				elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i.86|x86")
					set(OPENSSL_TARGET_PLATFORM "linux-x86")
				elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
					set(OPENSSL_TARGET_PLATFORM "linux-aarch64")
				elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm.*")
					set(OPENSSL_TARGET_PLATFORM "linux-armv4")
				elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
					set(OPENSSL_TARGET_PLATFORM "linux64-riscv64")
				elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc.*")
					set(OPENSSL_TARGET_PLATFORM "linux-ppc")
				elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "mips.*")
					set(OPENSSL_TARGET_PLATFORM "linux-mips32")
				endif()
			endif()
		endif()

		if(APPLE)
			list(APPEND OPENSSL_CONFIGURE_OPTIONS
				"CC=${CMAKE_C_COMPILER}"
			)
			if(CMAKE_AR)
				list(APPEND OPENSSL_CONFIGURE_OPTIONS "AR=${CMAKE_AR}")
			endif()
			if(CMAKE_RANLIB)
				list(APPEND OPENSSL_CONFIGURE_OPTIONS "RANLIB=${CMAKE_RANLIB}")
			endif()
			if(CMAKE_OSX_DEPLOYMENT_TARGET)
				list(APPEND OPENSSL_CONFIGURE_OPTIONS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
			endif()
			if(CMAKE_OSX_SYSROOT)
				list(APPEND OPENSSL_CONFIGURE_OPTIONS "-isysroot" "${CMAKE_OSX_SYSROOT}")
			endif()
		elseif(CMAKE_CROSSCOMPILING)
			list(APPEND OPENSSL_CONFIGURE_OPTIONS
				"CC=${CMAKE_C_COMPILER}"
				"AR=${CMAKE_AR}"
				"RANLIB=${CMAKE_RANLIB}"
			)
			if(CMAKE_SYSROOT)
				list(APPEND OPENSSL_CONFIGURE_OPTIONS "--sysroot=${CMAKE_SYSROOT}")
			endif()
		endif()

		if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
			set(OPENSSL_JOBS "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
		else()
			include(ProcessorCount)
			ProcessorCount(NCORES)
			if(NCORES EQUAL 0)
				set(NCORES 2)
			endif()
			set(OPENSSL_JOBS "${NCORES}")
		endif()

		set(OPENSSL_BUILD_CMD make -j${OPENSSL_JOBS} build_sw)
		set(OPENSSL_INSTALL_CMD make install_sw)
	endif()

	set(OPENSSL_CONFIGURE_CMD
		"${PERL_EXECUTABLE}"
		"${openssl_src_SOURCE_DIR}/Configure"
		${OPENSSL_TARGET_PLATFORM}
		"--prefix=${OPENSSL_INSTALL_DIR}"
		"--openssldir=${OPENSSL_INSTALL_DIR}/ssl"
		"--libdir=lib"
		${OPENSSL_CONFIGURE_OPTIONS}
	)

	ExternalProject_Add(openssl_build
		PREFIX            "${CMAKE_BINARY_DIR}/deps/openssl-${OPENSSL_VERSION}-ep"
		SOURCE_DIR        "${openssl_src_SOURCE_DIR}"
		BINARY_DIR        "${OPENSSL_BUILD_DIR}"
		STAMP_DIR         "${CMAKE_BINARY_DIR}/deps/openssl-${OPENSSL_VERSION}-ep/stamp"
		TMP_DIR           "${CMAKE_BINARY_DIR}/deps/openssl-${OPENSSL_VERSION}-ep/tmp"
		DOWNLOAD_COMMAND  ""
		UPDATE_COMMAND    ""
		CONFIGURE_COMMAND ${OPENSSL_CONFIGURE_CMD}
		BUILD_COMMAND     ${OPENSSL_BUILD_CMD}
		INSTALL_COMMAND   ${OPENSSL_INSTALL_CMD}
		BUILD_BYPRODUCTS  "${OPENSSL_SSL_LIBRARY}" "${OPENSSL_CRYPTO_LIBRARY}"
	)
endif()

# Standard imported targets
add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
set_target_properties(OpenSSL::Crypto PROPERTIES
	IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
	INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INSTALL_DIR}/include"
)

add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
set_target_properties(OpenSSL::SSL PROPERTIES
	IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
	INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INSTALL_DIR}/include"
)

target_link_libraries(OpenSSL::SSL INTERFACE OpenSSL::Crypto)

if(WIN32)
	target_link_libraries(OpenSSL::Crypto INTERFACE ws2_32 crypt32 advapi32 user32 gdi32)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux|Android")
	target_link_libraries(OpenSSL::Crypto INTERFACE pthread dl)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	target_link_libraries(OpenSSL::Crypto INTERFACE pthread)
endif()

if(TARGET openssl_build)
	add_dependencies(OpenSSL::Crypto openssl_build)
	add_dependencies(OpenSSL::SSL openssl_build)
endif()

list(APPEND EXTERNAL_DEPS OpenSSL::SSL OpenSSL::Crypto)
set(OPENSSL_INCLUDE_DIR "${OPENSSL_INSTALL_DIR}/include")
