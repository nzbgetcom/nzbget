if(CMAKE_SYSTEM_PROCESSOR MATCHES "i?86|x86_64")
	set(SSE2_CXXFLAGS "-msse2")
	set(SSSE3_CXXFLAGS "-mssse3")
	set(PCLMUL_CXXFLAGS "-msse4.1 -mpclmul")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64") 
	set(ACLECRC_CXXFLAGS "-march=armv8-a+crc -fpermissive") 
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm.*") 
	set(NEON_CXXFLAGS "-mfpu=neon") 
	set(ACLECRC_CXXFLAGS "-march=armv8-a+crc -fpermissive") 
endif()

add_library(regex STATIC
	${CMAKE_SOURCE_DIR}/lib/regex/regex.c
)
target_include_directories(regex PUBLIC
	${INCLUDES}
	${CMAKE_SOURCE_DIR}/lib/regex
)

set_source_files_properties(
	${CMAKE_SOURCE_DIR}/lib/yencode/Sse2Decoder.cpp
	PROPERTIES COMPILE_FLAGS "${SSE2_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_SOURCE_DIR}/lib/yencode/Ssse3Decoder.cpp
	PROPERTIES COMPILE_FLAGS "${SSSE3_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_SOURCE_DIR}/lib/yencode/PclmulCrc.cpp
	PROPERTIES COMPILE_FLAGS "${PCLMUL_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_SOURCE_DIR}/lib/yencode/NeonDecoder.cpp
	PROPERTIES COMPILE_FLAGS "${NEON_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_SOURCE_DIR}/lib/yencode/AcleCrc.cpp
	PROPERTIES COMPILE_FLAGS "${ACLECRC_CXXFLAGS}"
)

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
	${CMAKE_SOURCE_DIR}/lib/regex
	${CMAKE_SOURCE_DIR}/daemon/main
	${INCLUDES}
)

set(LIBS ${LIBS} regex yencode)
set(INCLUDES ${INCLUDES} ${CMAKE_SOURCE_DIR}/lib/regex ${CMAKE_SOURCE_DIR}/lib/yencode)
