option(ENABLE_TESTS "Enable tests")
option(DISABLE_TLS "Disable TLS")

message(STATUS "Options:")
message(STATUS "  BUILD_TYPE:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  ENABLE_TESTS: ${ENABLE_TESTS}")
message(STATUS "  DISABLE_TLS:  ${DISABLE_TLS}")

target_link_libraries(${PACKAGE} PRIVATE Par2)
target_include_directories(${PACKAGE} PRIVATE ${CMAKE_SOURCE_DIR}/lib/par2)

if(NOT DISABLE_TLS)
	find_package(OpenSSL REQUIRED)
	set(HAVE_OPENSSL 1)
	set(HAVE_X509_CHECK_HOST 1)
	target_link_libraries(${PACKAGE} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
	target_include_directories(${PACKAGE} PRIVATE ${OPENSSL_INCLUDE_DIR})
endif()

find_package(ZLIB REQUIRED)
target_link_libraries(${PACKAGE} PRIVATE ZLIB::ZLIB)
target_include_directories(${PACKAGE} PRIVATE ${ZLIB_INCLUDE_DIRS})

target_include_directories(${PACKAGE} PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/windows
	${CMAKE_SOURCE_DIR}/windows/resources
)
target_sources(${PACKAGE} PRIVATE ${CMAKE_SOURCE_DIR}/windows/resources/nzbget.rc)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib)

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

if(NOT DISABLE_SIGCHLD_HANDLER)
	set(SIGCHLD_HANDLER 1) 
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(_CRTDBG_MAP_ALLOC 1) 
endif()
