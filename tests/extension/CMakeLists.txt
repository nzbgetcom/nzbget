file(GLOB ExtensionSrc
	main.cpp
	ManifestFileTest.cpp
	ExtensionLoader.cpp
	ExtensionTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ManifestFile.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/ExtensionLoader.cpp
	${CMAKE_SOURCE_DIR}/daemon/extension/Extension.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Json.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp 
	${CMAKE_SOURCE_DIR}/lib/regex/regex.c
)

if (NOT WIN32)
	set(ExtensionSrc ${ExtensionSrc} ${CMAKE_BINARY_DIR}/code_revision.cpp)
endif()

add_executable(ExtensionTests ${ExtensionSrc})
target_link_libraries(ExtensionTests PRIVATE
	ZLIB::ZLIB
	Yencode
	${LIBXML2_LIBRARIES}
)

target_include_directories(ExtensionTests PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/daemon/extension
	${CMAKE_SOURCE_DIR}/lib/regex
	${CMAKE_SOURCE_DIR}/lib/yencode
	${ZLIB_INCLUDE_DIR}
	${OPENSSL_INCLUDE_DIR}
	${Boost_INCLUDE_DIR}
)

file(COPY ../testdata/extension/manifest DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ../testdata/extension/V1 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME ExtensionTests COMMAND $<TARGET_FILE:ExtensionTests> --log_level=message)
