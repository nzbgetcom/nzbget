set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(
	par2-turbo
	GIT_REPOSITORY  https://github.com/nzbgetcom/par2cmdline-turbo.git
	GIT_TAG         nzbget
	TLS_VERIFY	    TRUE
	GIT_SHALLOW     TRUE
	GIT_PROGRESS	TRUE
)

add_compile_definitions(HAVE_CONFIG_H PARPAR_ENABLE_HASHER_MD5CRC)
set(BUILD_TOOL OFF CACHE BOOL "")
set(BUILD_LIB ON CACHE BOOL "")
FetchContent_MakeAvailable(par2-turbo)

set(LIBS ${LIBS} par2-turbo gf16 hasher)
set(INCLUDES ${INCLUDES} ${par2_SOURCE_DIR}/include)
