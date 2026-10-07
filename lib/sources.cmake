if(NOT HAVE_SYSTEM_REGEX_H)
	add_library(regex STATIC
		${CMAKE_SOURCE_DIR}/lib/regex/regex.c
	)
	target_include_directories(regex PUBLIC
		${CMAKE_SOURCE_DIR}/lib/regex
		${CMAKE_SOURCE_DIR}
		${CMAKE_BINARY_DIR}
	)
	apply_compiler_flags(regex)
	apply_sanitizers(regex)
	if(TARGET libnzbget)
		target_link_libraries(libnzbget PUBLIC regex)
	endif()
endif()
