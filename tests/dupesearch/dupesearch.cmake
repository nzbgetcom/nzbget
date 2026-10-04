list(APPEND TESTS_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/dupesearch/ReleaseName.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/dupesearch/Newznab.cpp
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/dupesearch DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
