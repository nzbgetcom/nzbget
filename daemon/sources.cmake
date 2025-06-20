set(SRC
	${CMAKE_SOURCE_DIR}/daemon/connect/Connection.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/TlsSocket.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/WebDownloader.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/HttpClient.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/NetworkSpeedTest.cpp

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
	${CMAKE_SOURCE_DIR}/daemon/postprocess/PostUnpackRenamer.cpp

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
	${CMAKE_SOURCE_DIR}/daemon/queue/Deobfuscation.cpp

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
	${CMAKE_SOURCE_DIR}/daemon/util/Benchmark.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/DataAnalytics.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/OpenSSL.cpp

	${CMAKE_SOURCE_DIR}/daemon/system/SystemInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/system/OS.cpp
	${CMAKE_SOURCE_DIR}/daemon/system/CPU.cpp
	${CMAKE_SOURCE_DIR}/daemon/system/Network.cpp
)

set(WIN32_SRC
	${CMAKE_SOURCE_DIR}/daemon/windows/StdAfx.cpp
	${CMAKE_SOURCE_DIR}/daemon/windows/WinConsole.cpp
	${CMAKE_SOURCE_DIR}/daemon/windows/WinService.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Utf8.cpp
)

if(WIN32) 
	set(SRC ${SRC} ${WIN32_SRC})
	set(INCLUDES ${INCLUDES} ${CMAKE_SOURCE_DIR}/windows)
endif()

set(INCLUDES ${INCLUDES} 
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
	${CMAKE_SOURCE_DIR}/daemon/system
	${CMAKE_SOURCE_DIR}/daemon/util
)
