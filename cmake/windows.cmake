option(ENABLE_TESTS "Enable tests")
option(DISABLE_TLS "Disable TLS")
option(DISABLE_GZIP "Disable gzip")

# Ensure CMAKE_SYSTEM_PROCESSOR matches the target architecture (x86 vs AMD64)
# Visual Studio generator defaults CMAKE_SYSTEM_PROCESSOR to the host processor (AMD64) even with -A Win32
if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86")
    set(CMAKE_SYSTEM_PROCESSOR "x86" CACHE STRING "Target processor architecture" FORCE)
endif()

# Windows 7 minimum target (0x0601), disable MSVC CRT deprecation warnings
add_compile_definitions(_WIN32_WINNT=0x0601 WINVER=0x0601 _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)

# Build external dependencies via FetchContent
if(NOT DISABLE_TLS)
	include(${CMAKE_SOURCE_DIR}/cmake/openssl.cmake)
endif()
if(NOT DISABLE_GZIP)
	include(${CMAKE_SOURCE_DIR}/cmake/zlib.cmake)
endif()
include(${CMAKE_SOURCE_DIR}/cmake/libxml2.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/boost.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/rapidyenc.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/par2-turbo.cmake)
include(${CMAKE_SOURCE_DIR}/lib/sources.cmake)

find_package(Threads REQUIRED)

target_link_libraries(libnzbget PUBLIC
	Threads::Threads
	Boost::boost
	LibXml2::LibXml2
	rapidyenc::rapidyenc
	par2-turbo::par2-turbo
	winmm.lib
	$<$<CONFIG:Debug>:dbghelp.lib>
)

if(NOT DISABLE_TLS)
	target_link_libraries(libnzbget PUBLIC OpenSSL::SSL OpenSSL::Crypto)
	set(HAVE_X509_CHECK_HOST 1)
endif()

if(NOT DISABLE_GZIP)
	target_link_libraries(libnzbget PUBLIC ZLIB::ZLIB)
endif()

target_include_directories(libnzbget PUBLIC
	${CMAKE_SOURCE_DIR}/daemon/windows
	${CMAKE_SOURCE_DIR}/platforms/windows/resources
)

set(FUNCTION_MACRO_NAME __FUNCTION__)
set(HAVE_CTIME_R_3 1)
set(HAVE_VARIADIC_MACROS 1)
set(HAVE_GETADDRINFO 1)
set(SOCKLEN_T socklen_t)
set(HAVE_REGEX_H 1)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(_CRTDBG_MAP_ALLOC 1)
endif()
