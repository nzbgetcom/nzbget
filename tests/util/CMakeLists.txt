file(GLOB UtilTestSrc
	main.cpp
	FileSystemTest.cpp
	UtilTest.cpp
	NStringTest.cpp
	JsonTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Json.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Xml.cpp 
)

add_executable(UtilTests ${UtilTestSrc})
target_link_libraries(UtilTests PRIVATE
	Regex
	Threads::Threads
	ZLIB::ZLIB
	Yencode
	LibXml2::LibXml2
)

target_include_directories(UtilTests PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/lib/regex
	${CMAKE_SOURCE_DIR}/lib/yencode
	${ZLIB_INCLUDE_DIR}
	${OPENSSL_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
	${LIBXML2_INCLUDE_DIR}
)

add_test(NAME UtilTests COMMAND $<TARGET_FILE:UtilTests> --log_level=message)
