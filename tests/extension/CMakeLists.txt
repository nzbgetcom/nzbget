set(ExtensionSrc
	main.cpp
	ManifestFileTest.cpp
	ExtensionLoaderTest.cpp
	ExtensionTest.cpp
	ExtensionManagerTest.cpp
	../suite/TestUtil.cpp
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
	${CMAKE_SOURCE_DIR}/daemon/util/OpenSSL.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/Unpack.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/ParParser.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/WebDownloader.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/Connection.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/TlsSocket.cpp
)

if(WIN32)
	set(ExtensionSrc ${ExtensionSrc} ${CMAKE_SOURCE_DIR}/daemon/util/Utf8.cpp)
endif()

add_executable(ExtensionTests ${ExtensionSrc})
target_link_libraries(ExtensionTests PRIVATE ${LIBS})
target_include_directories(ExtensionTests PRIVATE ${INCLUDES} ../suite)

file(COPY ../testdata/extension/manifest DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ../testdata/extension/V1 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ../testdata/extension/scripts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME ExtensionTests COMMAND $<TARGET_FILE:ExtensionTests> --log_level=message)
