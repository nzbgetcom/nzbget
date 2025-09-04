set(HealthCheckTestsSrc
	main.cpp
	ChecksTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/healthcheck/HealthMonitor.cpp
	${CMAKE_SOURCE_DIR}/daemon/healthcheck/Check.cpp
	${CMAKE_SOURCE_DIR}/daemon/healthcheck/Checks.cpp
	${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Json.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/Xml.cpp
	${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp
)

add_executable(HealthCheckTests ${HealthCheckTestsSrc})

target_link_libraries(HealthCheckTests PRIVATE ${LIBS})
target_include_directories(HealthCheckTests PRIVATE ${INCLUDES})
if (TARGET ${PACKAGE})
	target_precompile_headers(HealthCheckTests REUSE_FROM ${PACKAGE})
else()
	target_precompile_headers(HealthCheckTests PRIVATE ${CMAKE_SOURCE_DIR}/daemon/main/nzbget.h)
endif()

add_test(NAME HealthCheckTests COMMAND $<TARGET_FILE:HealthCheckTests> --log_level=message)
