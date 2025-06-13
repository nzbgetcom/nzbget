option(ENABLE_TESTS "Enable tests")
option(DISABLE_TLS "Disable TLS")

message(STATUS "TOOLCHAIN OPTIONS:")
message(STATUS "  SYSTEM NAME      ${CMAKE_SYSTEM_NAME}")
message(STATUS "  SYSTEM PROCESSOR ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  TARGET TRIPLET   ${VCPKG_TARGET_TRIPLET}")
message(STATUS "BUILD OPTIONS:")
message(STATUS "  BUILD TYPE:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  ENABLE TESTS: ${ENABLE_TESTS}")
message(STATUS "  DISABLE TLS:  ${DISABLE_TLS}")

set(Boost_USE_STATIC_LIBS ON)

find_package(Threads REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(LIBS ${LIBS} dbghelp.lib)
endif()

set(LIBS ${LIBS} Threads::Threads Boost::json LibXml2::LibXml2 winmm.lib)
set(INCLUDES ${INCLUDES} ${Boost_INCLUDE_DIR} ${LIBXML2_INCLUDE_DIR})

if(NOT DISABLE_TLS)
	find_package(OpenSSL REQUIRED)
	set(HAVE_X509_CHECK_HOST 1)
	set(LIBS ${LIBS} OpenSSL::SSL OpenSSL::Crypto)
	set(INCLUDES ${INCLUDES} ${OPENSSL_INCLUDE_DIR})
endif()

find_package(ZLIB REQUIRED)
set(LIBS ${LIBS} ZLIB::ZLIB)
set(INCLUDES ${INCLUDES} ${ZLIB_INCLUDE_DIRS})
set(INCLUDES ${INCLUDES} 
	${CMAKE_SOURCE_DIR}/daemon/windows
	${CMAKE_SOURCE_DIR}/windows/resources
)

include(${CMAKE_SOURCE_DIR}/lib/sources.cmake)

include(${CMAKE_SOURCE_DIR}/cmake/par2-turbo.cmake)
add_dependencies(yencode par2-turbo)
if(NOT HAVE_SYSTEM_REGEX_H)
	add_dependencies(regex par2-turbo)
endif()

set(FUNCTION_MACRO_NAME __FUNCTION__)
set(HAVE_CTIME_R_3 1)
set(HAVE_VARIADIC_MACROS 1)
set(HAVE_GETADDRINFO 1)
set(SOCKLEN_T socklen_t)
set(HAVE_REGEX_H 1)
set(HAVE_STDINT_H 1)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(__amd64__ 1)
else()
	set(__i686__ 1) 
	set(_USE_32BIT_TIME_T 1) 
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(_CRTDBG_MAP_ALLOC 1) 
endif()
