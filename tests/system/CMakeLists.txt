set(SystemTestsSrc
	main.cpp
	SystemInfoTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/system/CPU.cpp 
	${CMAKE_SOURCE_DIR}/daemon/system/Network.cpp 
	${CMAKE_SOURCE_DIR}/daemon/system/OS.cpp 
	${CMAKE_SOURCE_DIR}/daemon/system/SystemInfo.cpp 
	${CMAKE_SOURCE_DIR}/daemon/connect/HttpClient.cpp 
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/queue/DownloadInfo.cpp 
	${CMAKE_SOURCE_DIR}/daemon/queue/DiskState.cpp 
	${CMAKE_SOURCE_DIR}/daemon/feed/FeedInfo.cpp 
	${CMAKE_SOURCE_DIR}/daemon/nntp/NewsServer.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Json.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Xml.cpp 
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/OpenSSL.cpp
)

if(WIN32)
	set(SystemTestsSrc ${SystemTestsSrc} ${CMAKE_SOURCE_DIR}/daemon/util/Utf8.cpp)
endif()

add_executable(SystemTests ${SystemTestsSrc})

target_link_libraries(SystemTests PRIVATE ${LIBS})
target_include_directories(SystemTests PRIVATE ${INCLUDES})

add_test(NAME SystemTests COMMAND $<TARGET_FILE:SystemTests> --log_level=message)
