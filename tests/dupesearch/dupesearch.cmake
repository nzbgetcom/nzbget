list(APPEND TESTS_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/dupesearch/ReleaseName.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/dupesearch/Newznab.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/dupesearch/Posting.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/dupesearch/DeadPostings.cpp
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/dupesearch DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
