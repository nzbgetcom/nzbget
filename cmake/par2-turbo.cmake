include(FetchContent)

set(PAR2_TURBO_VERSION_TAG "v1.5.0-20261005")

# Configure par2cmdline-turbo options before populating
set(BUILD_LIB ON CACHE BOOL "" FORCE)
set(BUILD_TOOL OFF CACHE BOOL "" FORCE)
set(ENABLE_CREATOR OFF CACHE BOOL "" FORCE)
set(ENABLE_PAR1 OFF CACHE BOOL "" FORCE)

FetchContent_Declare(par2_turbo
	GIT_REPOSITORY https://github.com/nzbgetcom/par2cmdline-turbo.git
	GIT_TAG        ${PAR2_TURBO_VERSION_TAG}
	GIT_SHALLOW    TRUE
	GIT_PROGRESS   TRUE
	EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(par2_turbo)

if(MSVC)
	set_target_properties(par2-turbo gf16 hasher PROPERTIES
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	)
endif()

if(USE_SANITIZERS)
	apply_sanitizers(par2-turbo)
	apply_sanitizers(gf16)
	apply_sanitizers(hasher)
endif()

if(ANDROID AND (CMAKE_SIZEOF_VOID_P EQUAL 4 OR ANDROID_ARCH MATCHES "armv7|x86"))
	foreach(target par2-turbo gf16 hasher)
		if(TARGET ${target})
			target_compile_options(${target} PRIVATE -U_FILE_OFFSET_BITS -D_FILE_OFFSET_BITS=32)
		endif()
	endforeach()
endif()

# The upstream project provides targets: par2-turbo, gf16, hasher
# Create a convenient alias matching the existing interface
add_library(par2-turbo::par2-turbo INTERFACE IMPORTED GLOBAL)
target_link_libraries(par2-turbo::par2-turbo INTERFACE par2-turbo gf16 hasher)

# Compile definitions required by the par2 headers used in ParChecker/ParRenamer
target_compile_definitions(par2-turbo::par2-turbo INTERFACE
	HAVE_CONFIG_H
	PARPAR_ENABLE_HASHER_MD5CRC
	PARPAR_INVERT_SUPPORT
	PARPAR_SLIM_GF16
)

# The upstream project provides targets: par2-turbo, gf16, hasher
# Add these as build dependencies for the main target
list(APPEND EXTERNAL_DEPS par2-turbo gf16 hasher)
