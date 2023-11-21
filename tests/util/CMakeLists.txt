file(GLOB FileSystemTestSrc
	FileSystemTest.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp 
	${CMAKE_SOURCE_DIR}/lib/regex/regex.c
)

file(GLOB NStringTestSrc
	NStringTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp
)

file(GLOB UtilTestSrc
	UtilTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp 
	${CMAKE_SOURCE_DIR}/lib/regex/regex.c
	${CMAKE_BINARY_DIR}/code_revision.cpp
)
if (NOT WIN32)
	set(FileSystemTestSrc ${FileSystemTestSrc} ${CMAKE_BINARY_DIR}/code_revision.cpp)
	set(NStringTestSrc ${NStringTestSrc} ${CMAKE_BINARY_DIR}/code_revision.cpp)
	set(UtilTestSrc ${UtilTestSrc} ${CMAKE_BINARY_DIR}/code_revision.cpp)
endif()

add_executable(FileSystemTest ${FileSystemTestSrc})
target_link_libraries(FileSystemTest PRIVATE 
	${Boost_UNIT_TEST_FRAMEWORK_LIBRARY_RELEASE} 
	ZLIB::ZLIB
	Yencode
	${LIBXML2_LIBRARIES}
)
target_include_directories(FileSystemTest PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/lib/regex
	${CMAKE_SOURCE_DIR}/lib/yencode
	${ZLIB_INCLUDE_DIR}
	${OPENSSL_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
)

add_executable(NStringTest ${NStringTestSrc})
target_link_libraries(NStringTest PRIVATE 
	${Boost_UNIT_TEST_FRAMEWORK_LIBRARY_RELEASE}
	${LIBXML2_LIBRARIES}
)
target_include_directories(NStringTest PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/util
	${OPENSSL_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
)

add_executable(UtilTest ${UtilTestSrc})
target_link_libraries(UtilTest PRIVATE
	ZLIB::ZLIB
	Yencode
	${Boost_UNIT_TEST_FRAMEWORK_LIBRARY_RELEASE}
	${LIBXML2_LIBRARIES}
)
target_include_directories(UtilTest PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/lib/regex
	${CMAKE_SOURCE_DIR}/lib/yencode
	${OPENSSL_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
)

add_test(NAME FileSystemTest COMMAND $<TARGET_FILE:FileSystemTest> --log_level=message)
add_test(NAME NStringTest COMMAND $<TARGET_FILE:NStringTest> --log_level=message)
add_test(NAME UtilTest COMMAND $<TARGET_FILE:UtilTest> --log_level=message)
