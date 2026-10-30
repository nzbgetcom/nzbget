list(APPEND TESTS_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/ServerPool.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/StatMeter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/Decoder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/YEncDecryptor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/YEncEncryptTestHelper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/ArticleWriter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/ArticleDownloader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nntp/ConformanceVectorsTest.cpp
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/test-vectors DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
