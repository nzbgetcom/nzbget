set(FeedTestsSrc
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
)

if(WIN32)
	set(FeedTestsSrc ${FeedTestsSrc} ${CMAKE_SOURCE_DIR}/daemon/util/Utf8.cpp)
endif()

add_executable(FeedTests ${FeedTestsSrc})

target_link_libraries(FeedTests PRIVATE ${LIBS})
target_include_directories(FeedTests PRIVATE ${INCLUDES})

file(COPY ../testdata/feed DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME FeedTests COMMAND $<TARGET_FILE:FeedTests> --log_level=message)
