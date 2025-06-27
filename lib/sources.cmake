if(IS_ARM)
	CHECK_CXX_COMPILER_FLAG("-mfpu=neon" COMPILER_SUPPORTS_ARM32_NEON)
	if(COMPILER_SUPPORTS_ARM32_NEON)
		set_source_files_properties(
			${CMAKE_SOURCE_DIR}/lib/yencode/NeonDecoder.cpp
			PROPERTIES COMPILE_FLAGS "-mfpu=neon"
		)
	else()
		CHECK_CXX_COMPILER_FLAG("-march=armv8-a+crc" COMPILER_SUPPORTS_ARM_CRC)
		set_source_files_properties(
			${CMAKE_SOURCE_DIR}/lib/yencode/AcleCrc.cpp
			PROPERTIES COMPILE_FLAGS "-march=armv8-a+crc"
		)
	endif()
endif()

if(IS_X86)
	CHECK_CXX_COMPILER_FLAG("-msse2" COMPILER_SUPPORTS_SSE2)
	if(COMPILER_SUPPORTS_SSE2)
		set_source_files_properties(
			${CMAKE_SOURCE_DIR}/lib/yencode/Sse2Decoder.cpp
			PROPERTIES COMPILE_FLAGS "-msse2"
		)
	endif()
	CHECK_CXX_COMPILER_FLAG("-mssse3" COMPILER_SUPPORTS_SSSE3)
	if(COMPILER_SUPPORTS_SSSE3)
		set_source_files_properties(
			${CMAKE_SOURCE_DIR}/lib/yencode/Ssse3Decoder.cpp
			PROPERTIES COMPILE_FLAGS "-mssse3"
		)
	endif()
	CHECK_CXX_COMPILER_FLAG("-msse4.1 -mpclmul" COMPILER_SUPPORTS_SSE41_PCLMUL)
	if(COMPILER_SUPPORTS_SSE41_PCLMUL)
		set_source_files_properties(
			${CMAKE_SOURCE_DIR}/lib/yencode/PclmulCrc.cpp
			PROPERTIES COMPILE_FLAGS "-msse4.1 -mpclmul"
		)
	endif()
endif()

if(NOT HAVE_SYSTEM_REGEX_H)
	add_library(regex STATIC
		${CMAKE_SOURCE_DIR}/lib/regex/regex.c
	)
	target_include_directories(regex PUBLIC
		${INCLUDES}
		${CMAKE_SOURCE_DIR}/lib/regex
	)
endif()

add_library(yencode STATIC
	${CMAKE_SOURCE_DIR}/lib/yencode/SimdInit.cpp
	${CMAKE_SOURCE_DIR}/lib/yencode/SimdDecoder.cpp
	${CMAKE_SOURCE_DIR}/lib/yencode/ScalarDecoder.cpp
	${CMAKE_SOURCE_DIR}/lib/yencode/Sse2Decoder.cpp
	${CMAKE_SOURCE_DIR}/lib/yencode/Ssse3Decoder.cpp
	${CMAKE_SOURCE_DIR}/lib/yencode/PclmulCrc.cpp
	${CMAKE_SOURCE_DIR}/lib/yencode/NeonDecoder.cpp
	${CMAKE_SOURCE_DIR}/lib/yencode/AcleCrc.cpp
	${CMAKE_SOURCE_DIR}/lib/yencode/SliceCrc.cpp
)
target_include_directories(yencode PUBLIC
	${CMAKE_SOURCE_DIR}/lib/yencode
	$<$<NOT:$<BOOL:${HAVE_SYSTEM_REGEX_H}>>:${CMAKE_SOURCE_DIR}/lib/regex>
	${CMAKE_SOURCE_DIR}/daemon/main
	${INCLUDES}
)

set(LIBS ${LIBS}
	$<$<NOT:$<BOOL:${HAVE_SYSTEM_REGEX_H}>>:regex>
	yencode
)
set(INCLUDES ${INCLUDES}
	$<$<NOT:$<BOOL:${HAVE_SYSTEM_REGEX_H}>>:${CMAKE_SOURCE_DIR}/lib/regex>
	${CMAKE_SOURCE_DIR}/lib/yencode
)
