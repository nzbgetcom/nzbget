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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(DEBUG 1)

	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
		add_compile_options(-Weverything -Wno-c++98-compat)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options(-Wall -Wextra)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		add_compile_options(/Zi /MTd /MP /EHs /W4 /utf-8)
	endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
	if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		add_compile_options(/MT /Oi /MP /EHs /GR- /W0 /utf-8)
	else()
		add_compile_options(-fno-rtti -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter)
		if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "powerpc")
			add_compile_options(-fstack-protector)
		endif()
	endif()

	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
		add_compile_options(-Wno-c++98-compat)
	endif()
endif()

include(ExternalProject)
include(CheckCXXCompilerFlag)
