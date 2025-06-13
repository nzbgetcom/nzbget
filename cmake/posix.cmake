option(ENABLE_STATIC "Build static executable")
option(ENABLE_TESTS "Build tests")
option(ENABLE_CLANG_TIDY "Enable Clang-Tidy static analizer")
option(ENABLE_SANITIZERS "Enable leak, undefined, address sanitizers")
option(DISABLE_TLS "Disable TLS")
option(DISABLE_CURSES "Disable curses")
option(DISABLE_GZIP "Disable gzip")
option(DISABLE_PARCHECK "Disable parcheck")

message(STATUS "TOOLCHAIN OPTIONS:")
message(STATUS "  SYSTEM NAME      ${CMAKE_SYSTEM_NAME}")
message(STATUS "  SYSTEM PROCESSOR ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "BUILD OPTIONS:")

message(STATUS "  BUILD TYPE:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  ENABLE STATIC:     ${ENABLE_STATIC}")
message(STATUS "  ENABLE TESTS:      ${ENABLE_TESTS}")
message(STATUS "  ENABLE SANITIZERS: ${ENABLE_SANITIZERS}")
message(STATUS "  DISABLE CURSES:    ${DISABLE_CURSES}")
message(STATUS "  DISABLE GZIP:      ${DISABLE_GZIP}")
message(STATUS "  DISABLE PARCHECK:  ${DISABLE_PARCHECK}")

if(APPLE)
	# On macOS Cmake, when cross-compiling, sometimes CMAKE_SYSTEM_PROCESSOR wrongfully stays
	# the same as CMAKE_HOST_SYSTEM_PROCESSOR regardless the target CPU.
	# The manual call to set(CMAKE_SYSTEM_PROCESSOR) has to be set after the project() call.
	# because project() might reset CMAKE_SYSTEM_PROCESSOR back to the value of CMAKE_HOST_SYSTEM_PROCESSOR.
	# Check if CMAKE_SYSTEM_PROCESSOR is not equal to CMAKE_OSX_ARCHITECTURES
	if(NOT CMAKE_OSX_ARCHITECTURES STREQUAL "")
		if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL CMAKE_OSX_ARCHITECTURES)
			# Split CMAKE_OSX_ARCHITECTURES into a list
			string(REPLACE ";" " " ARCH_LIST ${CMAKE_OSX_ARCHITECTURES})
			separate_arguments(ARCH_LIST UNIX_COMMAND ${ARCH_LIST})
			# Count the number of architectures
			list(LENGTH ARCH_LIST ARCH_COUNT)
			# Ensure that exactly one architecture is specified
			if(NOT ARCH_COUNT EQUAL 1)
				message(FATAL_ERROR "CMAKE_OSX_ARCHITECTURES must have exactly one value. Current value: ${CMAKE_OSX_ARCHITECTURES}")
			endif()
			set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_OSX_ARCHITECTURES})
			message(STATUS "CMAKE_SYSTEM_PROCESSOR is manually set to ${CMAKE_SYSTEM_PROCESSOR}")
		endif()
	endif()
endif()

if(ENABLE_CLANG_TIDY)
	set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*)
endif()

if(ENABLE_SANITIZERS)
	set(SANITIZERS_OPTION "-fsanitize=leak,undefined,address")
	target_compile_options(${PACKAGE} PRIVATE ${SANITIZERS_OPTION})
	target_link_options(${PACKAGE} PRIVATE ${SANITIZERS_OPTION})
endif()

if(ENABLE_STATIC)
	# due to the error "ld: library not found for -crt0.o" when using Apple Clang with "-static"
	if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
		set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-static" CACHE STRING "" FORCE)
		set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -static" CACHE STRING "" FORCE)
	endif()

	set(BUILD_SHARED_LIBS OFF)
	set(LIBS ${LIBS} $ENV{LIBS})
	set(INCLUDES ${INCLUDES} $ENV{INCLUDES})

	# for the sub-projects
	include_directories($ENV{INCLUDES})
else()
	find_package(Threads REQUIRED)
	find_package(LibXml2 REQUIRED)

	set(LIBS ${LIBS} Threads::Threads LibXml2::LibXml2)
	set(INCLUDES ${INCLUDES} ${LIBXML2_INCLUDE_DIR})

	if(NOT DISABLE_TLS)
		find_package(OpenSSL REQUIRED)
		set(LIBS ${LIBS} OpenSSL::SSL OpenSSL::Crypto)
		set(INCLUDES ${INCLUDES} ${OPENSSL_INCLUDE_DIR})
	endif()

	if(NOT DISABLE_CURSES)
		set(CURSES_NEED_NCURSES TRUE)
		if(NOT APPLE)
			set(CURSES_NEED_WIDE TRUE)
		endif()
		find_package(Curses REQUIRED)
		set(INCLUDES ${INCLUDES} ${CURSES_INCLUDE_DIRS})
		set(LIBS ${LIBS} ${CURSES_LIBRARIES})
	endif()

	if(NOT DISABLE_GZIP)
		find_package(ZLIB REQUIRED)
		set(INCLUDES ${INCLUDES} ${ZLIB_INCLUDE_DIRS})
		set(LIBS ${LIBS} ZLIB::ZLIB)
	endif()

	find_package(Boost COMPONENTS json)

	if(NOT Boost_JSON_FOUND)
		message(STATUS "The Boost library will be installed from github")

		include(${CMAKE_SOURCE_DIR}/cmake/boost.cmake)

		add_dependencies(${PACKAGE} boost)
	else()
		set(LIBS ${LIBS} Boost::json)
		set(INCLUDES ${INCLUDES} ${Boost_INCLUDE_DIR})
	endif()
endif()

include(CheckIncludeFiles)
check_include_files(regex.h HAVE_SYSTEM_REGEX_H)

include(${CMAKE_SOURCE_DIR}/lib/sources.cmake)

if(NOT DISABLE_PARCHECK)
	include(${CMAKE_SOURCE_DIR}/cmake/par2-turbo.cmake)
	add_dependencies(yencode par2-turbo)
	if(NOT HAVE_SYSTEM_REGEX_H)
		add_dependencies(regex par2-turbo)
	endif()
endif()

include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckTypeSize)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

check_include_files(sys/prctl.h HAVE_SYS_PRCTL_H)
check_include_files(regex.h HAVE_REGEX_H)
check_include_files(endian.h HAVE_ENDIAN_H) 
check_include_files(getopt.h HAVE_GETOPT_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdio.h HAVE_STDIO_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(alloca.h HAVE_ALLOCA_H)

check_library_exists(pthread pthread_create "" HAVE_PTHREAD_CREATE) 
check_library_exists(socket socket "" HAVE_SOCKET) 
check_library_exists(nsl inet_addr "" HAVE_INET_ADDR) 
check_library_exists(resolv hstrerror "" HAVE_HSTRERROR)

check_symbol_exists(lockf unistd.h HAVE_LOCKF) 
check_symbol_exists(pthread_cancel pthread.h HAVE_PTHREAD_CANCEL)
check_symbol_exists(F_FULLFSYNC fcntl.h HAVE_FULLFSYNC)

check_function_exists(getopt_long HAVE_GETOPT_LONG)
check_function_exists(fdatasync HAVE_FDATASYNC) 

set(SIGCHLD_HANDLER 1)

if(NOT DISABLE_CURSES)
	set(HAVE_NCURSES_H 1)
endif()

if(NOT DISABLE_PARCHECK)
	check_type_size(size_t SIZE_T)
	check_function_exists(fseeko HAVE_FSEEKO)
	check_function_exists(getopt HAVE_GETOPT)
else() 
  set(DISABLE_PARCHECK 1)
endif()

# check ctime_r
check_cxx_source_compiles("
  #include <time.h> 
  int main() 
	{
		time_t clock; 
		char buf[26]; 
		ctime_r(&clock, buf, 26); 
		return 0; 
	}" HAVE_CTIME_R_3)

if(NOT HAVE_CTIME_R_3) 
	check_cxx_source_compiles("
	#include <time.h> 
	int main() 
	{
		time_t clock; 
		char buf[26]; 
		ctime_r(&clock, buf); 
		return 0; 
	}" HAVE_CTIME_R_2)
endif()
if(NOT HAVE_CTIME_R_2) 
	message(FATAL_ERROR "ctime_r function not found") 
endif()

check_function_exists(getaddrinfo HAVE_GETADDRINFO) 
if(NOT HAVE_GETADDRINFO) 
	check_library_exists(nsl getaddrinfo "" HAVE_GETADDRINFO) 
endif()

# check gethostbyname_r, if getaddrinfo is not available
if(NOT HAVE_GETADDRINFO) 
	check_cxx_source_compiles("
	#include <netdb.h> 
	int main() 
	{ 
		char* szHost; 
		struct hostent hinfobuf; 
		char* strbuf; 
		int h_errnop; 
		struct hostent* hinfo = gethostbyname_r(szHost, &hinfobuf, strbuf, 1024, &h_errnop); 
		return 0; 
	}" HAVE_GETHOSTBYNAME_R_5) 
if(NOT HAVE_GETHOSTBYNAME_R_5) 
	check_cxx_source_compiles("
	#include <netdb.h> 
	int main() 
	{ 
		char* szHost; 
		struct hostent* hinfo; 
		struct hostent hinfobuf; 
		char* strbuf; 
		int h_errnop; 
		int err = gethostbyname_r(szHost, &hinfobuf, strbuf, 1024, &hinfo, &h_errnop); 
		return 0; 
	}" HAVE_GETHOSTBYNAME_R_6) 
if(NOT HAVE_GETHOSTBYNAME_R_6) 
	check_cxx_source_compiles("
	#include <netdb.h> 
	int main() 
	{ 
		char* szHost; 
		struct hostent hinfo; 
		struct hostent_data hinfobuf; 
		int err = gethostbyname_r(szHost, &hinfo, &hinfobuf); 
		return 0; 
	}" HAVE_GETHOSTBYNAME_R_3) 
if(NOT HAVE_GETHOSTBYNAME_R_3) 
  message(FATAL_ERROR "gethostbyname_r function not found") 
endif() 
endif() 
endif() 
if (NOT HAVE_GETHOSTBYNAME_R_3) 
	set(HAVE_GETHOSTBYNAME_R 1) 
	check_library_exists(nsl gethostbyname_r "" HAVE_GETHOSTBYNAME_R) 
endif() 
endif()

# Determine what socket length (socklen_t) data type is
check_cxx_source_compiles("
	#include <stddef.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	int main()
	{
		(void)getsockopt (1, 1, 1, NULL, (socklen_t*)NULL);
	}" SOCKLEN)
if(SOCKLEN)
  set(SOCKLEN_T socklen_t)
else() 
  check_cxx_source_compiles("
	#include <stddef.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	int main()
	{
		(void)getsockopt (1, 1, 1, NULL, (size_t*)NULL);
	}" SOCKLEN)
if(SOCKLEN)
  set(SOCKLEN_T size_t)
else() 
	check_cxx_source_compiles("
	#include <stddef.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	int main()
	{
		(void)getsockopt (1, 1, 1, NULL, (int*)NULL);
	}" SOCKLEN)
if(SOCKLEN)
  set(SOCKLEN_T int)
else()
  set(SOCKLEN_T int)
endif()
endif() 
endif()

# Check CPU cores via sysconf
check_cxx_source_compiles("
	#include <unistd.h>
	int main()
	{
		int a = _SC_NPROCESSORS_ONLN;
	}" HAVE_SC_NPROCESSORS_ONLN)

# Check TLS/SSL
if(NOT DISABLE_TLS AND NOT ENABLE_STATIC)
	check_library_exists(OpenSSL::Crypto X509_check_host "" HAVE_X509_CHECK_HOST)
elseif(NOT DISABLE_TLS AND ENABLE_STATIC)
	set(HAVE_X509_CHECK_HOST 1)
endif()

check_cxx_source_compiles("
	#include <stdio.h>
	int main() 
	{
		printf(\"%s\", __FUNCTION__);
		return 0;
	}" FUNCTION_MACRO_NAME_ONE)

check_cxx_source_compiles("
	#include <stdio.h>
	int main() 
	{
		printf(\"%s\", __func__);
		return 0;
	}" FUNCTION_MACRO_NAME_TWO)

if(FUNCTION_MACRO_NAME_ONE)
	set(FUNCTION_MACRO_NAME __FUNCTION__)
elseif (FUNCTION_MACRO_NAME_TWO)
	set(FUNCTION_MACRO_NAME __func__)
endif()

check_cxx_source_compiles("
	#define macro(...) macrofunc(__VA_ARGS__) 
	int macrofunc(int a, int b) { return a + b; }
	int main() 
	{ 
		int a = macro(1, 2);
		return 0;
	}" HAVE_VARIADIC_MACROS)

if(HAVE_VARIADIC_MACROS)
	set(DHAVE_VARIADIC_MACROS 1)
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
	check_cxx_source_compiles("
		#include <execinfo.h>
		#include <stdio.h>
		#include <stdlib.h>
		int main() 
		{ 
			void* array[100];
			size_t size; 
			char** strings; 
			size = backtrace(array, 100);
			strings = backtrace_symbols(array, size);
			return 0; 
		}" HAVE_BACKTRACE)
endif()

check_cxx_source_compiles("
	#include <atomic>
	int main()
	{
		std::atomic<uint64_t> x{ 0 };
		x.load(std::memory_order_acquire);
		return 0;
	}
" HAVE_NATIVE_ATOMICS_SUPPORT)

if(NOT HAVE_NATIVE_ATOMICS_SUPPORT)
	message(STATUS "Compiler lacks native support for C++ atomics. Linking against libatomic.")
	set(LIBS ${LIBS} -latomic)
endif()
