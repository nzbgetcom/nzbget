list(APPEND TESTS_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/network/Error.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/network/Url.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/network/Dial.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/network/Connection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/network/LineReader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/network/HostResolver.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/network/TlsContexts.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/network/DnsCache.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/network/http/Parser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/network/http/HttpClient.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/network/http/HttpClientTls.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/network/NetworkComponent.cpp
)
