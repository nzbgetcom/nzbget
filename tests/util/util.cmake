list(APPEND TESTS_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/util/FileSystem.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/Util.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/NString.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/Json.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/Benchmark.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/DataAnalytics.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util/Unpack.cpp
)

if(WIN32)
	list(APPEND TESTS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/util/Utf8.cpp)
endif()
