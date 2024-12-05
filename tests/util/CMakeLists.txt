set(UtilTestSrc
	main.cpp
	FileSystemTest.cpp
	UtilTest.cpp
	NStringTest.cpp
	JsonTest.cpp
	BenchmarkTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Json.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Xml.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Benchmark.cpp
)

if(WIN32)
	set(UtilTestSrc ${UtilTestSrc} 
		Utf8Test.cpp
		${CMAKE_SOURCE_DIR}/daemon/util/Utf8.cpp
	)
endif()

add_executable(UtilTests ${UtilTestSrc})

target_link_libraries(UtilTests PRIVATE	${LIBS})
target_include_directories(UtilTests PRIVATE ${INCLUDES})

add_test(NAME UtilTests COMMAND $<TARGET_FILE:UtilTests> --log_level=message)
