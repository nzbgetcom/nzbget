/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2023-2024 Denis <denis@nzbget.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#ifndef NZBGET_H
#define NZBGET_H

#include "config.h"

/***************** DEFINES FOR WINDOWS *****************/
#ifdef WIN32

/* Suppress warnings */
#define _CRT_SECURE_NO_DEPRECATE

/* Suppress warnings */
#define _CRT_NONSTDC_NO_WARNINGS

#ifndef _WIN64
#define _USE_32BIT_TIME_T
#endif

#if _WIN32_WINNT < 0x0501
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#ifdef _WIN64
#define __amd64__
#else
#define __i686__
#endif

#ifdef _DEBUG
// detection of memory leaks
#define _CRTDBG_MAP_ALLOC
#endif

#pragma warning(disable:4800) // 'type' : forcing value to bool 'true' or 'false' (performance warning)
#pragma warning(disable:4267) // 'var' : conversion from 'size_t' to 'type', possible loss of data

#define popen _wpopen
#define pclose _pclose

#endif

/***************** GLOBAL INCLUDES *****************/

#ifdef WIN32
// WINDOWS INCLUDES

// Using "WIN32_LEAN_AND_MEAN" to disable including of many unneeded headers
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <winsvc.h>
#include <direct.h>
#include <shlobj.h>
#include <dbghelp.h>
#include <mmsystem.h>
#include <io.h>
#include <process.h>
#include <WinIoCtl.h>
#include <wincon.h>
#include <shellapi.h>
#include <winreg.h>
#include <comutil.h>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#else

// POSIX INCLUDES

#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/file.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>

#ifndef __linux__
#include <sys/sysctl.h>
#endif

#define __BSD__ (__FreeBSD__ || __NetBSD__ || __OpenBSD__ || __DragonFly__)

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#endif /* POSIX INCLUDES */

// COMMON INCLUDES

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include <inttypes.h>

#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <list>
#include <set>
#include <map>
#include <unordered_map>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <utility>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <chrono>
#include <optional>
#include <variant>
#include <limits>
#include <type_traits>
#include <random>
#include <exception>
#include <iomanip>

#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlerror.h>
#include <libxml/entities.h>
#include <libxml/tree.h>

#include <boost/asio.hpp>
#ifndef DISABLE_TLS
#include <boost/asio/ssl.hpp>
#include "OpenSSL.h"
#endif

// NOTE: do not include <iostream> in "nzbget.h". <iostream> contains objects requiring
// intialization, causing every unit in nzbget to have initialization routine. This in particular
// is causing fatal problems in SIMD units which must not have static initialization because
// they contain code with runtime CPU dispatching.
//#include <iostream>

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#ifndef DISABLE_GZIP
#include <zlib.h>
#endif

#ifndef DISABLE_PARCHECK
#include <assert.h>
#include <cassert>
#endif /* NOT DISABLE_PARCHECK */


/***************** GLOBAL FUNCTION AND CONST OVERRIDES *****************/

#ifdef WIN32

// WINDOWS

#ifndef strdup
#define strdup _strdup
#endif
#define fdopen _fdopen
#define ctime_r(timep, buf, bufsize) ctime_s(buf, bufsize, timep)
#define gmtime_r(time, tm) gmtime_s(tm, time)
#define strtok_r(str, delim, saveptr) strtok_s(str, delim, saveptr)
#define strerror_r(errnum, buffer, size) strerror_s(buffer, size, errnum)
#define mkdir(dir, flags) _mkdir(dir)
#define rmdir _rmdir
#define strcasecmp(a, b) stricmp(a, b)
#define strncasecmp(a, b, c) strnicmp(a, b, c)
#define __S_ISTYPE(mode, mask) (((mode) & _S_IFMT) == (mask))
#define S_ISDIR(mode) __S_ISTYPE((mode), _S_IFDIR)
#define S_ISREG(mode) __S_ISTYPE((mode), _S_IFREG)
#define S_DIRMODE nullptr
#define socklen_t int
#define SHUT_WR 0x01
#define SHUT_RDWR 0x02
#define PATH_SEPARATOR '\\'
#define ALT_PATH_SEPARATOR '/'
#define LINE_ENDING "\r\n"
#define atoll _atoi64
#define fseek _fseeki64
#define ftell _ftelli64
// va_copy is available in vc2013 and onwards
#if _MSC_VER < 1800
#define va_copy(d,s) ((d) = (s))
#endif
#ifndef FSCTL_SET_SPARSE
#define FSCTL_SET_SPARSE 590020
#endif
#define FOPEN_RB "rbN"
#define FOPEN_RBP "rb+N"
#define FOPEN_WB "wbN"
#define FOPEN_AB "abN"

#define __SSE2__
#define __SSSE3__
#define __PCLMUL__

#ifdef DEBUG
// redefine "exit" to avoid printing memory leaks report when terminated because of wrong command line switches
#define exit(code) ExitProcess(code)
#endif

#else

// POSIX

#define closesocket(sock) close(sock)
#define SOCKET int
#define INVALID_SOCKET (-1)
#define PATH_SEPARATOR '/'
#define ALT_PATH_SEPARATOR '\\'
#define MAX_PATH 1024
#define S_DIRMODE (S_IRWXU | S_IRWXG | S_IRWXO)
#define LINE_ENDING "\n"
#define FOPEN_RB "rb"
#define FOPEN_RBP "rb+"
#define FOPEN_WB "wb"
#define FOPEN_AB "ab"
#define CHILD_WATCHDOG 1
#define fseek fseeko

#endif /* POSIX */

// COMMON DEFINES FOR ALL PLATFORMS
#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

#ifdef HAVE_STDINT_H
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
#else
typedef unsigned char uint8;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed int int32;
typedef unsigned int uint32;
typedef signed long long int64;
typedef unsigned long long uint64;
#endif

#ifndef PRId64
#define PRId64 "lld"
#endif
#ifndef PRIi64
#define PRIi64 "lli"
#endif
#ifndef PRIu64
#define PRIu64 "llu"
#endif

typedef unsigned char uchar;

// Assume little endian if byte order is not defined
#ifndef __BYTE_ORDER
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#ifdef __GNUC__
#define PRINTF_SYNTAX(strindex) __attribute__ ((format (printf, strindex, strindex+1)))
#define SCANF_SYNTAX(strindex) __attribute__ ((format (scanf, strindex, strindex+1)))
#else
#define PRINTF_SYNTAX(strindex)
#define SCANF_SYNTAX(strindex)
#endif

// providing "std::make_unique" for GCC 4.8.x (only 4.8.x)
#if __GNUC__ && __cplusplus < 201402L && __cpp_generic_lambdas < 201304
namespace std {
template<class T> struct _Unique_if { typedef unique_ptr<T> _Single_object; };
template<class T> struct _Unique_if<T[]> { typedef unique_ptr<T[]> _Unknown_bound; };
template<class T, class... Args> typename _Unique_if<T>::_Single_object make_unique(Args&&... args) {
	return unique_ptr<T>(new T(std::forward<Args>(args)...));
}
template<class T> typename _Unique_if<T>::_Unknown_bound make_unique(size_t n) {
	typedef typename remove_extent<T>::type U;
	return unique_ptr<T>(new U[n]());
}
}
#endif

#endif /* NZBGET_H */
