cmake_minimum_required(VERSION 3.22)

set(VERSION "22.2")

project(
	nzbget
	VERSION ${VERSION}
	DESCRIPTION "NZBGet is a binary downloader, which downloads files from Usenet"
	LANGUAGES C CXX
)

option(ENABLE_TESTS "Enable tests" ON)

set(PACKAGE "nzbget")
add_compile_definitions(HAVE_CONFIG_H=1)

configure_file(
	${CMAKE_SOURCE_DIR}/cmake_config.h.in
	${CMAKE_BINARY_DIR}/config.h
)

set(SOURCES
	${CMAKE_SOURCE_DIR}/daemon/connect/Connection.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/TlsSocket.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/WebDownloader.cpp

	${CMAKE_SOURCE_DIR}/daemon/extension/CommandScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/FeedScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/NzbScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/PostScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/PostScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/QueueScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ScanScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/SchedulerScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ScriptConfig.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ScriptConfig.cpp

	${CMAKE_SOURCE_DIR}/daemon/feed/FeedCoordinator.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedFile.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedFilter.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedInfo.cpp

	${CMAKE_SOURCE_DIR}/daemon/frontend/ColoredFrontend.cpp
	${CMAKE_SOURCE_DIR}/daemon/frontend/Frontend.cpp
	${CMAKE_SOURCE_DIR}/daemon/frontend/LoggableFrontend.cpp
	${CMAKE_SOURCE_DIR}/daemon/frontend/NCursesFrontend.cpp

	${CMAKE_SOURCE_DIR}/daemon/main/CommandLineParser.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/DiskService.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Maintenance.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/nzbget.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Scheduler.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/StackTrace.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/WorkState.cpp

	${CMAKE_SOURCE_DIR}/daemon/nntp/ArticleDownloader.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/ArticleWriter.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/Decoder.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NewsServer.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NntpConnection.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/ServerPool.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/StatMeter.cpp

	${CMAKE_SOURCE_DIR}/daemon/nserv/NntpServer.cpp
	${CMAKE_SOURCE_DIR}/daemon/nserv/NServFrontend.cpp
	${CMAKE_SOURCE_DIR}/daemon/nserv/NServMain.cpp
	${CMAKE_SOURCE_DIR}/daemon/nserv/NzbGenerator.cpp
	${CMAKE_SOURCE_DIR}/daemon/nserv/YEncoder.cpp

	${CMAKE_SOURCE_DIR}/daemon/postprocess/Cleanup.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/DirectUnpack.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/DupeMatcher.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/ParChecker.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/ParParser.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/ParRenamer.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/PrePostProcessor.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/RarReader.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/RarRenamer.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/Rename.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/Repair.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/Unpack.cpp

	${CMAKE_SOURCE_DIR}/daemon/queue/DirectRenamer.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DupeCoordinator.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/HistoryCoordinator.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/NzbFile.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/QueueCoordinator.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/QueueEditor.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/Scanner.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/UrlCoordinator.cpp

	${CMAKE_SOURCE_DIR}/daemon/remote/BinRpc.cpp
	${CMAKE_SOURCE_DIR}/daemon/remote/RemoteClient.cpp
	${CMAKE_SOURCE_DIR}/daemon/remote/RemoteServer.cpp
	${CMAKE_SOURCE_DIR}/daemon/remote/WebServer.cpp
	${CMAKE_SOURCE_DIR}/daemon/remote/XmlRpc.cpp
	${CMAKE_SOURCE_DIR}/daemon/remote/MessageBase.h

	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Observer.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/ScriptController.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Service.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Thread.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
)

SET(WIN32_SOURCES
	${CMAKE_SOURCE_DIR}/daemon/windows/StdAfx.cpp
	${CMAKE_SOURCE_DIR}/daemon/windows/WinConsole.cpp
	${CMAKE_SOURCE_DIR}/daemon/windows/WinService.cpp
)

if (WIN32)
	set(SOURCES ${SOURCES} ${WIN32_SOURCES})
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF)
set(OPENSSL_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibXml2 REQUIRED)

if (NOT WIN32)
	find_package(Curses REQUIRED)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang|AppleClang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Weverything")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /W4")
	set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} winmm.lib /NODEFAULTLIB:msvcrt.lib;libcmt.lib;msvcrtd.lib")
endif()

include_directories(lib/regex)
include_directories(${CMAKE_BINARY_DIR})
add_subdirectory(lib)

add_executable(nzbget ${SOURCES})
target_include_directories(nzbget PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/connect
	${CMAKE_SOURCE_DIR}/daemon/extension
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/frontend
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/daemon/nserv
	${CMAKE_SOURCE_DIR}/daemon/postprocess
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/lib/par2
	${CMAKE_SOURCE_DIR}/lib/regex
	${CMAKE_SOURCE_DIR}/lib/yencode
	${Boost_INCLUDE_DIR}
	${LIBXML2_INCLUDE_DIR}
	${CURSES_INCLUDE_DIR}
)
if (WIN32)
	target_include_directories(nzbget PRIVATE
		${CMAKE_SOURCE_DIR}/daemon/windows
		${CMAKE_SOURCE_DIR}/windows/resources
	)
	target_sources(nzbget PRIVATE ${CMAKE_SOURCE_DIR}/windows/resources/nzbget.rc)
endif()

target_link_libraries(nzbget PRIVATE
	Yencode
	Par2
	Regex
	OpenSSL::SSL
	OpenSSL::Crypto
	ZLIB::ZLIB
	LibXml2::LibXml2
	${CURSES_LIBRARIES}
)

if(ENABLE_TESTS)
	include(CTest)
	add_subdirectory(tests)
endif()
