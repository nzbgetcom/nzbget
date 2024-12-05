set(QueueTestsSrc
	NzbFileTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/NzbFile.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp 
	${CMAKE_SOURCE_DIR}/daemon/nntp/NewsServer.cpp
)

if(WIN32)
	set(QueueTestsSrc ${QueueTestsSrc} ${CMAKE_SOURCE_DIR}/daemon/util/Utf8.cpp)
endif()

add_executable(QueueTests ${QueueTestsSrc})

target_link_libraries(QueueTests PRIVATE ${LIBS})
target_include_directories(QueueTests PRIVATE ${INCLUDES})

file(COPY ../testdata/nzbfile DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME QueueTests COMMAND $<TARGET_FILE:QueueTests> --log_level=message)
