list(APPEND TESTS_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/queue/NzbFile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/Deobfuscation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/ArchiveProcessor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/queue/DirectRenamer.cpp
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/nzbfile DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
