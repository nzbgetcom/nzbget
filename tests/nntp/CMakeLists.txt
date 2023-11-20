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
	${CMAKE_SOURCE_DIR}/lib/regex/regex.c
)
target_link_libraries(ServerPoolTest PRIVATE 
	${Boost_UNIT_TEST_FRAMEWORK_LIBRARY_RELEASE}
	OpenSSL::SSL
	ZLIB::ZLIB
	Par2
	Yencode
)
target_include_directories(ServerPoolTest PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/daemon/connect
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/lib/yencode
	${CMAKE_SOURCE_DIR}/lib/par2
	${OPENSSL_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
)

add_test(NAME ServerPoolTest COMMAND $<TARGET_FILE:ServerPoolTest> --log_level=message)
