set(NNTPTestsSrc
	main.cpp
	ServerPoolTest.cpp
	StatMeterTest.cpp
	DecoderTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/WorkState.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/ServerPool.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/StatMeter.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NewsServer.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NntpConnection.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/Decoder.cpp
	${CMAKE_SOURCE_DIR}/daemon/nserv/YEncoder.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Observer.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/Connection.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/TlsSocket.cpp
)

if(WIN32)
	set(NNTPTestsSrc ${NNTPTestsSrc} ${CMAKE_SOURCE_DIR}/daemon/util/Utf8.cpp)
endif()

add_executable(NNTPTests ${NNTPTestsSrc})

target_link_libraries(NNTPTests PRIVATE ${LIBS})
target_include_directories(NNTPTests PRIVATE ${INCLUDES})

add_test(NAME NNTPTests COMMAND $<TARGET_FILE:NNTPTests> --log_level=message)
