# In-tree build for par2-turbo via FetchContent.
# Builds the nzbgetcom fork from the pinned tag and exposes it as
# the native CMake target par2-turbo::par2-turbo.

include(FetchContent)

set(PAR2_TURBO_VERSION_TAG "v1.5.0-20260914")

FetchContent_Declare(par2_turbo
    GIT_REPOSITORY https://github.com/nzbgetcom/par2cmdline-turbo.git
    GIT_TAG        ${PAR2_TURBO_VERSION_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(par2_turbo)

if(MSVC)
    set_target_properties(par2-turbo gf16 hasher PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
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