add_executable(NzbFileTest
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
target_link_libraries(NzbFileTest PRIVATE ${LIBS})
target_include_directories(NzbFileTest PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/lib/yencode
	${INCLUDES}
)

file(COPY ../testdata/nzbfile DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME NzbFileTest COMMAND $<TARGET_FILE:NzbFileTest> --log_level=message)
