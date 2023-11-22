file(GLOB PostprocessTestsSrc
	main.cpp
	DirectUnpackTest.cpp
	DupeMatcherTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/DirectUnpack.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/DupeMatcher.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Thread.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Script.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp
	${CMAKE_SOURCE_DIR}/lib/regex/regex.c
)

if (NOT WIN32)
	set(PostprocessTests ${PostprocessTestsSrc} ${CMAKE_BINARY_DIR}/code_revision.cpp)
endif()

add_executable(PostprocessTests ${PostprocessTests})
target_link_libraries(PostprocessTests PRIVATE
	ZLIB::ZLIB
	Yencode
)
target_include_directories(PostprocessTests PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/postprocess
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/daemon/connect
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/lib/yencode
	${Boost_INCLUDE_DIR}
)

file(COPY ../testdata/rarrenamer DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME PostprocessTests COMMAND $<TARGET_FILE:PostprocessTests> --log_level=message)
