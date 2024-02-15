if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
		set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Weverything" CACHE STRING "" FORCE)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall -Wextra" CACHE STRING "" FORCE)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /MP /W4 /DDEBUG /D_DEBUG /wd4800 /wd4267" CACHE STRING "" FORCE)
		set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} winmm.lib Dbghelp.lib libcpmtd.lib" CACHE STRING "" FORCE)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug" CACHE STRING "" FORCE)
	endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
		set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -Weverything" CACHE STRING "" FORCE)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -Wall -Wextra" CACHE STRING "" FORCE)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MP /W4 /DNDEBUG /wd4800 /wd4267" CACHE STRING "" FORCE)
		set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} winmm.lib" CACHE STRING "" FORCE)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)
	endif()
endif()

if(NOT MSVC)
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "i?86|x86_64")
		set(SSE2_CXXFLAGS "-msse2")
		set(SSSE3_CXXFLAGS "-mssse3")
		set(PCLMUL_CXXFLAGS "-msse4.1 -mpclmul")
	elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm*")
		set(NEON_CXXFLAGS "-mfpu=neon")
		set(ACLECRC_CXXFLAGS "-march=armv8-a+crc -fpermissive")
	elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
		set(ACLECRC_CXXFLAGS "-march=armv8-a+crc -fpermissive")
	endif()
endif()

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)

include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(${CMAKE_SOURCE_DIR}/lib)

set(SRC
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
	${CMAKE_SOURCE_DIR}/daemon/extension/Extension.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ExtensionLoader.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ExtensionManager.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ManifestFile.cpp

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
	${CMAKE_SOURCE_DIR}/daemon/util/Json.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Xml.cpp
)

set(WIN32_SRC
	${CMAKE_SOURCE_DIR}/daemon/windows/StdAfx.cpp
	${CMAKE_SOURCE_DIR}/daemon/windows/WinConsole.cpp
	${CMAKE_SOURCE_DIR}/daemon/windows/WinService.cpp
)

if(WIN32) 
	set(SRC ${SRC} ${WIN32_SRC})
endif()

add_executable(${PACKAGE} ${SRC})

target_link_libraries(${PACKAGE} PRIVATE
	Yencode 
	Regex 
	Threads::Threads 
	Boost::json 
	ZLIB::ZLIB 
	LibXml2::LibXml2
)
target_include_directories(${PACKAGE} PRIVATE
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
	${CMAKE_SOURCE_DIR}/lib/regex
	${CMAKE_SOURCE_DIR}/lib/yencode
	${Boost_INCLUDE_DIR}
	${LIBXML2_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIRS}
)
