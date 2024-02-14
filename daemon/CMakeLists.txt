set(SRC
	${CMAKE_CURRENT_SOURCE_DIR}/connect/Connection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/connect/TlsSocket.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/connect/WebDownloader.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/extension/CommandScript.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/FeedScript.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/NzbScript.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/PostScript.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/PostScript.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/QueueScript.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/ScanScript.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/SchedulerScript.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/ScriptConfig.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/ScriptConfig.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/Extension.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/ExtensionLoader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/ExtensionManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/ManifestFile.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/feed/FeedCoordinator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/feed/FeedFile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/feed/FeedFilter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/feed/FeedInfo.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/frontend/ColoredFrontend.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/frontend/Frontend.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/frontend/LoggableFrontend.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/frontend/NCursesFrontend.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/main/CommandLineParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/main/DiskService.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/main/Maintenance.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/main/nzbget.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/main/Options.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/main/Scheduler.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/main/StackTrace.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/main/WorkState.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/nntp/ArticleDownloader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/ArticleWriter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/Decoder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/NewsServer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/NntpConnection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/ServerPool.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/StatMeter.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/nserv/NntpServer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nserv/NServFrontend.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nserv/NServMain.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nserv/NzbGenerator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nserv/YEncoder.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/Cleanup.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/DirectUnpack.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/DupeMatcher.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/ParChecker.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/ParParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/ParRenamer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/PrePostProcessor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/RarReader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/RarRenamer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/Rename.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/Repair.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/Unpack.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/queue/DirectRenamer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/DiskState.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/DownloadInfo.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/DupeCoordinator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/HistoryCoordinator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/NzbFile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/QueueCoordinator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/QueueEditor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/Scanner.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/UrlCoordinator.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/remote/BinRpc.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteClient.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/RemoteServer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/WebServer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/XmlRpc.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/remote/MessageBase.h

	${CMAKE_CURRENT_SOURCE_DIR}/util/FileSystem.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/Log.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/NString.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/Observer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/ScriptController.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/Service.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/Thread.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/Util.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/Json.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/Xml.cpp
)

set(WIN32_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/windows/StdAfx.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/windows/WinConsole.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/windows/WinService.cpp
)

if(WIN32)
	set(SRC ${SRC} ${WIN32_SRC} PARENT_SCOPE)
endif()

set_property(GLOBAL PROPERTY SRC ${SRC})
