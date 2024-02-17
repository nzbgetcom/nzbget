add_library(Regex STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/regex/regex.c
)
target_include_directories(Regex PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/regex
)

add_library(Yencode STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/SimdInit.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/SimdDecoder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/ScalarDecoder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/Sse2Decoder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/Ssse3Decoder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/PclmulCrc.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/NeonDecoder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/AcleCrc.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/SliceCrc.cpp
)
target_include_directories(Yencode PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/yencode
	${CMAKE_CURRENT_SOURCE_DIR}/regex
	${CMAKE_SOURCE_DIR}/daemon/main
	${LIBXML2_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIR}
	${OPENSSL_INCLUDE_DIR}
	${PC_GNUTLS_INCLUDE_DIRS}
)

set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/Sse2Decoder.cpp
	PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${SSE2_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/Ssse3Decoder.cpp
	PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${SSSE3_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/PclmulCrc.cpp
	PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${PCLMUL_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/NeonDecoder.cpp
	PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} ${NEON_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/AcleCrc.cpp
	PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${ACLECRC_CXXFLAGS}"
)

if(NOT DISABLE_PARCHECK)
	add_library(Par2 STATIC
		${CMAKE_CURRENT_SOURCE_DIR}/par2/commandline.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/crc.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/creatorpacket.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/criticalpacket.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/datablock.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/descriptionpacket.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/diskfile.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/filechecksummer.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/galois.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/mainpacket.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/md5.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/par2fileformat.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/par2repairer.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/par2repairersourcefile.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/parheaders.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/recoverypacket.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/reedsolomon.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/verificationhashtable.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/par2/verificationpacket.cpp
	)
	target_include_directories(Par2 PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/par2
		${CMAKE_CURRENT_SOURCE_DIR}/regex
		${CMAKE_SOURCE_DIR}/daemon/main
		${CMAKE_SOURCE_DIR}/daemon/util
		${LIBXML2_INCLUDE_DIR}
		${Iconv_INCLUDE_DIR}
		${Boost_INCLUDE_DIR}
		${OPENSSL_INCLUDE_DIR}
		${GNUTLS_INCLUDE_DIR}
	)
endif()
