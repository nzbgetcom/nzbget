add_executable(CommandLineParserTest
	CommandLineParserTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/CommandLineParser.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NewsServer.cpp
	${CMAKE_SOURCE_DIR}/lib/regex/regex.c
)
target_link_libraries(CommandLineParserTest PRIVATE 
	${Boost_UNIT_TEST_FRAMEWORK_LIBRARY_RELEASE}
	ZLIB::ZLIB
	Yencode
)
target_include_directories(CommandLineParserTest PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/lib/yencode
	${CMAKE_SOURCE_DIR}/lib/regex
	${OPENSSL_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
)

add_executable(OptionsTest
	OptionsTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/CommandLineParser.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/nntp/NewsServer.cpp
	${CMAKE_SOURCE_DIR}/lib/regex/regex.c
)
target_link_libraries(OptionsTest PRIVATE 
	${Boost_UNIT_TEST_FRAMEWORK_LIBRARY_RELEASE}
	ZLIB::ZLIB
	Yencode
)
target_include_directories(OptionsTest PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/lib/yencode
	${CMAKE_SOURCE_DIR}/lib/regex
	${OPENSSL_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
)

add_test(NAME CommandLineParserTest COMMAND $<TARGET_FILE:CommandLineParserTest> --log_level=message)
add_test(NAME OptionsTest COMMAND $<TARGET_FILE:OptionsTest> --log_level=message)
