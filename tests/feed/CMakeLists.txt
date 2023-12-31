add_executable(FeedTest
	FeedFilterTest.cpp
	FeedFileTest.cpp
	main.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedFilter.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedFile.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp 
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NewsServer.cpp
	${CMAKE_SOURCE_DIR}/lib/regex/regex.c
)
target_link_libraries(FeedTest PRIVATE
	ZLIB::ZLIB
	Yencode
	LibXml2::LibXml2
)
target_include_directories(FeedTest PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/lib/regex
	${Boost_INCLUDE_DIR}
	${LIBXML2_INCLUDE_DIR}
)

file(COPY ../testdata/feed DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME FeedTest COMMAND $<TARGET_FILE:FeedTest> --log_level=message)
