option(ENABLE_TESTS "Enable tests")
option(DISABLE_TLS "Disable TLS")

message(STATUS "TOOLCHAIN OPTIONS:")
message(STATUS "  SYSTEM NAME      ${CMAKE_SYSTEM_NAME}")
message(STATUS "  SYSTEM PROCESSOR ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "BUILD OPTIONS:")
message(STATUS "  BUILD TYPE:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  ENABLE LTO:      ${ENABLE_LTO}")
message(STATUS "  ENABLE TESTS:    ${ENABLE_TESTS}")
message(STATUS "  DISABLE TLS:     ${DISABLE_TLS}")
message(STATUS "  USE SANITIZERS:  ${USE_SANITIZERS}")

# Windows 7 minimum target (0x0601), disable MSVC CRT deprecation warnings, disable Boost auto-linking
add_compile_definitions(_WIN32_WINNT=0x0601 WINVER=0x0601 _CRT_SECURE_NO_WARNINGS BOOST_ALL_NO_LIB)

# Build external dependencies via FetchContent
include(${CMAKE_SOURCE_DIR}/cmake/openssl.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/zlib.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/libxml2.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/boost.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/rapidyenc.cmake)

find_package(Threads REQUIRED)

# nzbget target is created in CMakeLists.txt before this include
# Use CMAKE_PROJECT_NAME which is "nzbget"
set(NZBGET_TARGET ${CMAKE_PROJECT_NAME})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_link_libraries(${NZBGET_TARGET} PUBLIC dbghelp.lib)
endif()

target_link_libraries(${NZBGET_TARGET} PUBLIC
	Threads::Threads
	Boost::boost
	LibXml2::LibXml2
	ZLIB::ZLIB
	rapidyenc::rapidyenc
	winmm.lib
)

if(NOT DISABLE_TLS)
	target_link_libraries(${NZBGET_TARGET} PUBLIC OpenSSL::SSL OpenSSL::Crypto)
	set(HAVE_X509_CHECK_HOST 1)
	if(OPENSSL_INCLUDE_DIR)
		target_include_directories(${NZBGET_TARGET} PUBLIC ${OPENSSL_INCLUDE_DIR})
	endif()
endif()

target_include_directories(${NZBGET_TARGET} PUBLIC
	${CMAKE_SOURCE_DIR}/daemon/windows
	${CMAKE_SOURCE_DIR}/platforms/windows/resources
)

include(${CMAKE_SOURCE_DIR}/lib/sources.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/par2-turbo.cmake)

target_link_libraries(${NZBGET_TARGET} PUBLIC par2-turbo::par2-turbo)

if(NOT HAVE_SYSTEM_REGEX_H)
	target_link_libraries(${NZBGET_TARGET} PUBLIC regex)
endif()

set(FUNCTION_MACRO_NAME __FUNCTION__)
set(HAVE_CTIME_R_3 1)
set(HAVE_VARIADIC_MACROS 1)
set(HAVE_GETADDRINFO 1)
set(SOCKLEN_T socklen_t)
set(HAVE_REGEX_H 1)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(__amd64__ 1)
else()
	set(__i686__ 1)
	set(_USE_32BIT_TIME_T 1)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(_CRTDBG_MAP_ALLOC 1)
endif()

# Set CMAKE_SYSTEM_PROCESSOR for cross-compilation on Windows
# Needed because CMake with VS generator doesn't auto-update it for -A Win32/x64
if(MSVC)
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		set(CMAKE_SYSTEM_PROCESSOR "x86" CACHE STRING "Target processor architecture" FORCE)
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(CMAKE_SYSTEM_PROCESSOR "AMD64" CACHE STRING "Target processor architecture" FORCE)
	endif()
endif()
