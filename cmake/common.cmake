if (CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86|x86_64|x64|amd64|AMD64|win32|Win32")
	set(IS_X86 TRUE)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|x64|amd64|AMD64")
		set(IS_X64 TRUE)
	endif()
endif()
if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm|ARM|aarch64|arm64|ARM64|armeb|aarch64be|aarch64_be")
	set(IS_ARM TRUE)
endif()
if (CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64|rv64")
	set(IS_RISCV64 TRUE)
endif()
if (CMAKE_SYSTEM_PROCESSOR MATCHES "riscv32|rv32")
	set(IS_RISCV32 TRUE)
endif()

include(ExternalProject)
include(CheckCXXCompilerFlag)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckTypeSize)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(CheckIPOSupported)

option(ENABLE_CCACHE "Enable ccache compiler launcher if available" ON)
if(ENABLE_CCACHE AND NOT WIN32)
	find_program(CCACHE_PROGRAM ccache)
	if(CCACHE_PROGRAM)
		set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
		set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
		message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
	endif()
endif()

option(ENABLE_LTO "Enable Link-Time Optimization (IPO)" OFF)
if(ENABLE_LTO)
	if(CMAKE_CROSSCOMPILING)
		# Cross-compilation: try_compile cannot run test binaries
		# LTO is known to work on supported platforms with LLVM Clang
		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
			set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
			message(STATUS "Link-Time Optimization (LTO/IPO) enabled (cross-compiling, assuming supported)")
		else()
			message(WARNING "LTO/IPO requested during cross-compilation but compiler support unknown")
		endif()
	else()
		check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
		if(IPO_SUPPORTED)
			set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
			message(STATUS "Link-Time Optimization (LTO/IPO) enabled")
		else()
			message(WARNING "LTO/IPO requested but not supported: ${IPO_ERROR}")
		endif()
	endif()
endif()

if(MSVC)
	# Multi-processor compilation, UTF-8 source encoding, and detailed diagnostics for all translation units
	add_compile_options(/MP /utf-8 /Zc:inline /diagnostics:column)

	# Optimization flags for Release builds (applies to all targets and dependencies)
	add_compile_options(
		$<$<NOT:$<CONFIG:Debug>>:/Gy>
		$<$<NOT:$<CONFIG:Debug>>:/Gw>
	)
	add_link_options(
		$<$<NOT:$<CONFIG:Debug>>:/OPT:REF>
		$<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>
		$<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>
		$<$<NOT:$<CONFIG:Debug>>:/DYNAMICBASE>
		$<$<NOT:$<CONFIG:Debug>>:/NXCOMPAT>
		$<$<NOT:$<CONFIG:Debug>>:/LARGEADDRESSAWARE>
		$<$<AND:$<NOT:$<CONFIG:Debug>>,$<BOOL:${IS_X64}>>:/CETCOMPAT>
	)
endif()

if(NOT MSVC)
	# Fortify Source (Level 3 where available, fallback to 2).
	# The fortified __*_chk symbols must be provided by libc; probe with a
	# real executable link, because the default try-compile builds static
	# libraries where undefined symbols never fail (musl has no __*_chk).
	set(_FORTIFY_TRY_TYPE ${CMAKE_TRY_COMPILE_TARGET_TYPE})
	set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
	check_library_exists(c __memcpy_chk "" HAVE_LIBC_FORTIFY)
	set(CMAKE_TRY_COMPILE_TARGET_TYPE ${_FORTIFY_TRY_TYPE})
	unset(_FORTIFY_TRY_TYPE)
	if(HAVE_LIBC_FORTIFY)
		if(NOT APPLE)
			check_cxx_compiler_flag("-Wp,-D_FORTIFY_SOURCE=3" HAVE_FORTIFY_3)
		endif()
	else()
		message(STATUS "libc lacks __*_chk symbols, skipping _FORTIFY_SOURCE")
	endif()

	if(NOT APPLE)
		# Stack clash protection is enabled on Linux only.
		# Not supported by the 32-bit ARM Android target (flag is ignored with a warning)
		if((CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID) AND NOT (ANDROID AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm"))
			check_cxx_compiler_flag("-fstack-clash-protection" HAVE_STACK_CLASH_PROTECT)
		endif()

		# Hardware Control-flow enforcement (Intel CET / ARM BTI - Linux ELF only)
		if(IS_X86 AND IS_X64)
			check_cxx_compiler_flag("-fcf-protection=full" HAVE_CF_PROTECT)
		elseif(IS_ARM AND NOT (ANDROID AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm"))
			check_cxx_compiler_flag("-mbranch-protection=standard" HAVE_BRANCH_PROTECT)
		endif()
	endif()

	# Stack protector
	check_cxx_compiler_flag("-fstack-protector-strong" HAVE_STACK_PROTECT)
endif()

# Apply strict compiler warnings and hardening flags to nzbget targets
# Third-party dependencies (FetchContent) do not inherit these flags
function(apply_compiler_flags target)
	# Hidden visibility for all non-MSVC builds:
	# enables compiler devirtualization, cross-TU inlining, dead vtable elimination,
	# and reduces binary size. Safe for a monolithic executable.
	if(NOT MSVC)
		target_compile_options(${target} PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
	endif()

	# macOS: reserve space in Mach-O header for codesign and install_name_tool
	if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
		target_link_options(${target} PRIVATE -Wl,-headerpad_max_install_names)
	endif()

	# Suppress OpenSSL deprecation warnings in Release-like builds
	target_compile_definitions(${target} PRIVATE
		$<$<NOT:$<CONFIG:Debug>>:OPENSSL_SUPPRESS_DEPRECATED>
	)

	# Compiler-specific warnings and options (multi-config generator aware via $<CONFIG:...>)
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
		target_compile_options(${target} PRIVATE
			$<$<CONFIG:Debug>:-Weverything>
			$<$<CONFIG:Debug>:-Wno-c++98-compat>
			$<$<CONFIG:Debug>:-Wno-c++98-compat-pedantic>
			$<$<NOT:$<CONFIG:Debug>>:-Wno-unused-function>
		)
		target_compile_definitions(${target} PRIVATE _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		target_compile_options(${target} PRIVATE
			$<$<CONFIG:Debug>:-Wall>
			$<$<CONFIG:Debug>:-Wextra>
			$<$<NOT:$<CONFIG:Debug>>:-Wno-unused-function>
		)
		target_compile_definitions(${target} PRIVATE
			$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:_GLIBCXX_ASSERTIONS>
		)
	elseif(MSVC)
		target_compile_options(${target} PRIVATE
			$<$<CONFIG:Debug>:/W4>
			$<$<CONFIG:Debug>:/we4477>
			$<$<CONFIG:Debug>:/we4473>
			$<$<NOT:$<CONFIG:Debug>>:/Oi>
			$<$<NOT:$<CONFIG:Debug>>:/guard:cf>
		)
		target_link_options(${target} PRIVATE
			$<$<NOT:$<CONFIG:Debug>>:/guard:cf>
			# 0x800 = LOAD_LIBRARY_SEARCH_SYSTEM32 (restricts statically linked DLL searches to System32, mitigating DLL hijacking)
			$<$<NOT:$<CONFIG:Debug>>:/DEPENDENTLOADFLAG:0x800>
		)
	endif()

	if(NOT MSVC)
		target_compile_options(${target} PRIVATE
			$<$<NOT:$<CONFIG:Debug>>:-fno-rtti>
			$<$<NOT:$<CONFIG:Debug>>:-ffunction-sections>
			$<$<NOT:$<CONFIG:Debug>>:-fdata-sections>
		)

		if(HAVE_LIBC_FORTIFY)
			if(HAVE_FORTIFY_3)
				target_compile_definitions(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=3>)
			else()
				target_compile_definitions(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)
			endif()
		endif()

		if(HAVE_STACK_CLASH_PROTECT)
			target_compile_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fstack-clash-protection>)
		endif()

		if(HAVE_CF_PROTECT)
			target_compile_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fcf-protection=full>)
		elseif(HAVE_BRANCH_PROTECT)
			target_compile_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-mbranch-protection=standard>)
		endif()

		if(NOT APPLE AND NOT WIN32)
			target_link_options(${target} PRIVATE
				$<$<NOT:$<CONFIG:Debug>>:-Wl,-z,relro,-z,now>
				$<$<NOT:$<CONFIG:Debug>>:-Wl,-z,noexecstack>
			)
		endif()

		if(HAVE_STACK_PROTECT)
			target_compile_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-fstack-protector-strong>)
		endif()

		if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
			target_link_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-Wl,-dead_strip>)
		else()
			target_link_options(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-Wl,--gc-sections>)
		endif()
	endif()
endfunction()

function(apply_sanitizers target)
	if(NOT USE_SANITIZERS)
		return()
	endif()

	if(MSVC)
		target_compile_options(${target} PRIVATE /fsanitize=address)
	else()
		target_compile_options(${target} PRIVATE
			-fsanitize=${USE_SANITIZERS}
			-fno-omit-frame-pointer
			-fno-sanitize-recover=all
		)
	endif()

	# Object libraries are not linked; their consumers apply the link flags.
	get_target_property(_target_type ${target} TYPE)
	if(_target_type STREQUAL "OBJECT_LIBRARY")
		return()
	endif()

	if(MSVC)
		target_link_options(${target} PRIVATE /fsanitize=address)
	else()
		target_link_options(${target} PRIVATE
			-fsanitize=${USE_SANITIZERS}
		)
	endif()
endfunction()
