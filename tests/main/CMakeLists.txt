set(MainTestsSrc
	main.cpp
	OptionsTest.cpp
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
)

if(WIN32)
	set(MainTestsSrc ${MainTestsSrc} ${CMAKE_SOURCE_DIR}/daemon/util/Utf8.cpp)
endif()

add_executable(MainTests ${MainTestsSrc})

target_link_libraries(MainTests PRIVATE ${LIBS})
target_include_directories(MainTests PRIVATE ${INCLUDES})

add_test(NAME MainTests COMMAND $<TARGET_FILE:MainTests> --log_level=message)
