set(SystemHealthTestsSrc
    main.cpp
    ValidatorsTest.cpp
    PathsValidatorTest.cpp
    NewsServerValidatorTest.cpp
	${CMAKE_SOURCE_DIR}/daemon/systemhealth/Status.cpp
    ${CMAKE_SOURCE_DIR}/daemon/systemhealth/Validators.cpp
    ${CMAKE_SOURCE_DIR}/daemon/systemhealth/SectionValidator.cpp
    ${CMAKE_SOURCE_DIR}/daemon/systemhealth/PathsValidator.cpp
    ${CMAKE_SOURCE_DIR}/daemon/systemhealth/NewsServerValidator.cpp
    ${CMAKE_SOURCE_DIR}/daemon/nntp/NewsServer.cpp
    ${CMAKE_SOURCE_DIR}/daemon/nntp/NntpConnection.cpp
    ${CMAKE_SOURCE_DIR}/daemon/connect/Connection.cpp
	${CMAKE_SOURCE_DIR}/daemon/connect/TlsSocket.cpp
    ${CMAKE_SOURCE_DIR}/daemon/main/Options.cpp
    ${CMAKE_SOURCE_DIR}/daemon/util/Xml.cpp
    ${CMAKE_SOURCE_DIR}/daemon/util/Json.cpp
    ${CMAKE_SOURCE_DIR}/daemon/util/NString.cpp
    ${CMAKE_SOURCE_DIR}/daemon/util/Util.cpp
    ${CMAKE_SOURCE_DIR}/daemon/util/FileSystem.cpp
    ${CMAKE_SOURCE_DIR}/daemon/util/Log.cpp
)

if(WIN32)
	set(SystemHealthTestsSrc ${SystemHealthTestsSrc} ${CMAKE_SOURCE_DIR}/daemon/util/Utf8.cpp)
endif()

add_executable(SystemHealthTests ${SystemHealthTestsSrc})

target_link_libraries(SystemHealthTests PRIVATE ${LIBS})
target_include_directories(SystemHealthTests PRIVATE ${INCLUDES})
if (TARGET ${PACKAGE})
    target_precompile_headers(SystemHealthTests REUSE_FROM ${PACKAGE})
else()
    target_precompile_headers(SystemHealthTests PRIVATE ${CMAKE_SOURCE_DIR}/daemon/main/nzbget.h)
endif()

add_test(NAME SystemHealthTests COMMAND $<TARGET_FILE:SystemHealthTests> --log_level=message)
