add_executable(ServerPoolTest
	ServerPoolTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/ServerPool.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NewsServer.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NntpConnection.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/Connection.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/TlsSocket.cpp
)
target_link_libraries(ServerPoolTest PRIVATE ${LIBS})
target_include_directories(ServerPoolTest PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/daemon/connect
	${CMAKE_SOURCE_DIR}/daemon/system
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/lib/yencode
	${CMAKE_SOURCE_DIR}/lib/par2
	${INCLUDES}
)

add_test(NAME ServerPoolTest COMMAND $<TARGET_FILE:ServerPoolTest> --log_level=message)
