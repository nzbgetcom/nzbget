file(GLOB PostprocessTestsSrc
	main.cpp
	DirectUnpackTest.cpp
	DupeMatcherTest.cpp
	ParCheckerTest.cpp
	ParRenamerTest.cpp
	RarReaderTest.cpp
	RarReaderTest.cpp
	RarRenamerTest.cpp
	../suite/TestUtil.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/DirectUnpack.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/DupeMatcher.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/ParChecker.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/ParParser.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/ParRenamer.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/RarReader.cpp
	${CMAKE_SOURCE_DIR}/daemon/postprocess/RarRenamer.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Thread.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/ScriptController.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp
)

add_executable(PostprocessTests ${PostprocessTestsSrc})
target_link_libraries(PostprocessTests PRIVATE ${LIBS})
target_include_directories(PostprocessTests PRIVATE
	../suite
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/postprocess
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/daemon/connect
	${CMAKE_SOURCE_DIR}/daemon/system
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/lib/yencode
	${CMAKE_SOURCE_DIR}/lib/par2
	${INCLUDES}
)

file(COPY ../testdata/rarrenamer DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME PostprocessTests COMMAND $<TARGET_FILE:PostprocessTests> --log_level=message)
