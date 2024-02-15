option(ENABLE_TESTS "Enable tests")
option(BUILD_SHARED_LIBS "Build shared libs")
option(DISABLE_TLS "Disable TLS")
option(DISABLE_CURSES "Disable curses")
option(DISABLE_GZIP "Disable gzip")
option(DISABLE_PARCHECK "Disable parcheck")
option(DISABLE_SIGCHLD_HANDLER "Do not use sigchld-handler")
if(NOT DISABLE_TLS)
	option(USE_OPENSSL "Use OpenSSL" ON)
	option(USE_GNUTLS "Use GnuTLS" OFF)
endif()

if(NOT DISABLE_TLS AND USE_GNUTLS)
	set(USE_OPENSSL OFF)
endif()

message(STATUS "Options:")
message(STATUS "  BUILD_TYPE:              ${CMAKE_BUILD_TYPE}")
message(STATUS "  ENABLE_TESTS:            ${ENABLE_TESTS}")
message(STATUS "  DISABLE_TLS:             ${DISABLE_TLS}")
message(STATUS "  - OPENSSL:               ${USE_OPENSSL}")
message(STATUS "  - GNUTLS:                ${USE_GNUTLS}")
message(STATUS "  DISABLE_CURSES:          ${DISABLE_CURSES}")
message(STATUS "  DISABLE_PARCHECK:        ${DISABLE_PARCHECK}")
message(STATUS "  DISABLE_SIGCHLD_HANDLER: ${DISABLE_SIGCHLD_HANDLER}")

if(NOT DISABLE_TLS)
	if(USE_OPENSSL AND NOT USE_GNUTLS)
		find_package(OpenSSL REQUIRED)
		target_link_libraries(${PACKAGE} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
		target_include_directories(${PACKAGE} PRIVATE ${OPENSSL_INCLUDE_DIR})
	elseif(USE_GNUTLS)
		find_package(GnuTLS REQUIRED)
		target_link_libraries(${PACKAGE} PRIVATE ${GNUTLS_LIBRARIES} nettle)
		target_include_directories(${PACKAGE} PRIVATE ${GNUTLS_INCLUDE_DIR})
	endif()
endif()

if(NOT DISABLE_CURSES)
	find_package(Curses REQUIRED)
	target_link_libraries(${PACKAGE} PRIVATE ${CURSES_LIBRARIES})
	target_include_directories(${PACKAGE} PRIVATE ${CURSES_INCLUDE_DIRS})
endif()

if(NOT DISABLE_PARCHECK)
	target_link_libraries(${PACKAGE} PRIVATE Par2)
	target_include_directories(${PACKAGE} PRIVATE ${CMAKE_SOURCE_DIR}/lib/par2)
endif()

if(BUILD_SHARED_LIBS)
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".dll" ".dylib")
else()
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
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
check_include_file(unvimistd.h HAVE_UNISTD_H)

check_library_exists(pthread pthread_create "" HAVE_PTHREAD_CREATE) 
check_library_exists(socket socket "" HAVE_SOCKET) 
check_library_exists(nsl inet_addr "" HAVE_INET_ADDR) 
check_library_exists(resolv hstrerror "" HAVE_HSTRERROR)

check_symbol_exists(lockf unistd.h HAVE_LOCKF) 
check_symbol_exists(pthread_cancel pthread.h HAVE_PTHREAD_CANCEL)
check_symbol_exists(F_FULLFSYNC fcntl.h HAVE_FULLFSYNC)

check_function_exists(getopt_long HAVE_GETOPT_LONG)
check_function_exists(fdatasync HAVE_FDATASYNC) 

if(NOT DISABLE_SIGCHLD_HANDLER)
	set(SIGCHLD_HANDLER 1) 
endif()

if(NOT DISABLED_PARCHECK)
	check_type_size(size_t SIZE_T)
	check_symbol_exists(fseeko "stdio.h" HAVE_FSEEKO)
	check_function_exists(stricmp HAVE_STRICMP)
	check_function_exists(getopt HAVE_GETOPT)
else() 
  set(DISABLED_PARCHECK 1)
endif()

if(NOT DISABLE_CURSES)
	check_include_file(ncurses.h HAVE_NCURSES_H)
if(NOT HAVE_NCURSES_H) 
	check_include_file(ncurses/ncurses.h HAVE_NCURSES_NCURSES_H) 
if(NOT HAVE_NCURSES_NCURSES_H) 
	check_include_file(curses.h HAVE_CURSES_H) 
if(NOT HAVE_CURSES_H) 
	message(FATAL_ERROR "Couldn't find curses headers (ncurses.h or curses.h)") 
endif() 
endif() 
endif()
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
if(USE_OPENSSL)   
	set(HAVE_OPENSSL 1)
			
	# Check if OpenSSL supports function "X509_check_host"
	check_library_exists(crypto X509_check_host "" HAVE_X509_CHECK_HOST)

elseif(USE_GNUTLS)
	# Check GnuTLS library
	check_library_exists(gnutls gnutls_global_init "" HAVE_LIBGNUTLS)

	# Check Nettle library
	check_library_exists(nettle nettle_pbkdf2_hmac_sha256 "" HAVE_NETTLE)
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
	#define macro(...)   macrofunc(__VA_ARGS__) 
	int macrofunc(int a, int b) { return a + b; }
	int main() 
	{ 
		int a = macro(1, 2);
		return 0;
	}" HAVE_VARIADIC_MACROS)

if(HAVE_VARIADIC_MACROS)
	set(DHAVE_VARIADIC_MACROS 1)
endif()

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
