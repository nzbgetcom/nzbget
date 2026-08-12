list(APPEND TESTS_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/CollectionAnalyzer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/Cleanup.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/DirectUnpack.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/DupeMatcher.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/RarReader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/RarRenamer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/postprocess/PostDownloadRenamer.cpp
)

if(NOT DISABLE_PARCHECK)
	list(APPEND TESTS_SRC
		${CMAKE_CURRENT_SOURCE_DIR}/postprocess/ParChecker.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/postprocess/ParRenamer.cpp
	)
endif()

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/dupematcher1 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/dupematcher2 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/parchecker DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/parcheckerUtf8 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/rarrenamer DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
