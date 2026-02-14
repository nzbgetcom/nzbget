list(APPEND TESTS_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/extension/ManifestFile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/ExtensionLoader.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/Extension.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/ExtensionManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/extension/ScanScript.cpp
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/extension/manifest DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/extension/V1 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/testdata/extension/scripts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
