file(GLOB ExtensionSrc
	main.cpp
	ManifestFileTest.cpp
	ExtensionLoaderTest.cpp
	ExtensionTest.cpp
	ExtensionManagerTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ManifestFile.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ExtensionLoader.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/Extension.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ExtensionManager.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/WorkState.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NewsServer.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Json.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Xml.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/ScriptController.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Thread.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Observer.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/Unpack.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/ParParser.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/WebDownloader.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/Connection.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/TlsSocket.cpp
)

add_executable(ExtensionTests ${ExtensionSrc})
target_link_libraries(ExtensionTests PRIVATE ${LIBS})

target_include_directories(ExtensionTests PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/daemon/extension
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/daemon/postprocess
	${CMAKE_SOURCE_DIR}/daemon/connect
	${CMAKE_SOURCE_DIR}/daemon/system
	${CMAKE_SOURCE_DIR}/lib/regex
	${CMAKE_SOURCE_DIR}/lib/yencode
	${INCLUDES}
)

file(COPY ../testdata/extension/manifest DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ../testdata/extension/V1 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ../testdata/extension/scripts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME ExtensionTests COMMAND $<TARGET_FILE:ExtensionTests> --log_level=message)
