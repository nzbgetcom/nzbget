if(NOT HAVE_SYSTEM_REGEX_H)
	add_library(regex STATIC
		${CMAKE_SOURCE_DIR}/lib/regex/regex.c
	)
	target_include_directories(regex PUBLIC
		${INCLUDES}
		${CMAKE_SOURCE_DIR}/lib/regex
	)
endif()

set(LIBS ${LIBS}
	$<$<NOT:$<BOOL:${HAVE_SYSTEM_REGEX_H}>>:regex>
)
