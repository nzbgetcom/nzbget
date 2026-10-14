include(FetchContent)

set(RAPIDYENC_VERSION_TAG "v1.1.1-20261006")

# Set CMake options for rapidyenc subproject (must be set before FetchContent_MakeAvailable)
set(DISABLE_SHARED ON CACHE BOOL "" FORCE)
set(DISABLE_TOOL ON CACHE BOOL "" FORCE)
set(DISABLE_ENCODE ON CACHE BOOL "" FORCE)
set(DISABLE_DECODE OFF CACHE BOOL "" FORCE)
set(DISABLE_CRC OFF CACHE BOOL "" FORCE)
set(DISABLE_AVX256 OFF CACHE BOOL "" FORCE)
set(DISABLE_CRCUTIL OFF CACHE BOOL "" FORCE)

# Disable ARMv8 CRC32/PMULL on 32-bit ARM for compatibility with ARMv7 and older kernels
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64")
	set(DISABLE_ARM_CRC ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(rapidyenc
	GIT_REPOSITORY https://github.com/nzbgetcom/rapidyenc.git
	GIT_TAG        ${RAPIDYENC_VERSION_TAG}
	GIT_SHALLOW    TRUE
	GIT_PROGRESS   TRUE
	EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(rapidyenc)

if(MSVC)
	set_target_properties(rapidyenc_static PROPERTIES
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	)
endif()

if(USE_SANITIZERS)
	apply_sanitizers(rapidyenc_static)
endif()

# The upstream project provides target: rapidyenc_static (static library)
# Header rapidyenc.h is in the source root directory - upstream doesn't export it, so we add it
add_library(rapidyenc::rapidyenc INTERFACE IMPORTED GLOBAL)
target_link_libraries(rapidyenc::rapidyenc INTERFACE rapidyenc_static)
target_include_directories(rapidyenc::rapidyenc INTERFACE ${rapidyenc_SOURCE_DIR})

list(APPEND EXTERNAL_DEPS rapidyenc_static)
