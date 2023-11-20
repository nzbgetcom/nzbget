add_executable(FeedFilterTest
	FeedFilterTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedFilter.cpp
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
target_link_libraries(FeedFilterTest PRIVATE 
	${Boost_UNIT_TEST_FRAMEWORK_LIBRARY_RELEASE} 
	ZLIB::ZLIB
	Yencode
)
target_include_directories(FeedFilterTest PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/lib/regex
	${Boost_INCLUDE_DIR}
)

add_test(NAME FeedFilterTest COMMAND $<TARGET_FILE:FeedFilterTest> --log_level=message)
