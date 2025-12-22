set(FeedTestsSrc
	FeedFilterTest.cpp
	FeedFileTest.cpp
	NzbInfoCreatorTest.cpp
	main.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedFilter.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedFile.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedCoordinator.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/WorkState.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/ScriptController.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Observer.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Thread.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/OpenSSL.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Service.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Json.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Xml.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Unpack.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/SevenZip.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Unrar.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/Scanner.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/UrlCoordinator.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DupeCoordinator.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/HistoryCoordinator.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/QueueCoordinator.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/QueueEditor.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/Deobfuscation.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DirectRenamer.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/NzbFile.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/WebDownloader.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/Connection.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/TlsSocket.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/Repair.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/PrePostProcessor.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/ParChecker.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/DirectUnpack.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/Cleanup.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/Rename.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/DupeMatcher.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/ParRenamer.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/RarRenamer.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/UnpackController.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/PostUnpackRenamer.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/PrePostProcessor.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/ParParser.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/RarReader.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NewsServer.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/ServerPool.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/ArticleWriter.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/ArticleDownloader.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NntpConnection.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/Decoder.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/StatMeter.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/FeedScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/NzbScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/QueueScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/PostScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ScanScript.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ExtensionManager.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ExtensionLoader.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ManifestFile.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/Extension.cpp
)

if(WIN32)
	set(FeedTestsSrc ${FeedTestsSrc} ${CMAKE_SOURCE_DIR}/daemon/util/Utf8.cpp)
endif()

add_executable(FeedTests ${FeedTestsSrc})

target_link_libraries(FeedTests PRIVATE ${LIBS})
target_include_directories(FeedTests PRIVATE ${INCLUDES})
if (TARGET ${PACKAGE})
	target_precompile_headers(FeedTests REUSE_FROM ${PACKAGE})
else()
	target_precompile_headers(FeedTests PRIVATE ${CMAKE_SOURCE_DIR}/daemon/main/nzbget.h)
endif()

file(COPY ../testdata/feed DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME FeedTests COMMAND $<TARGET_FILE:FeedTests> --log_level=message)
