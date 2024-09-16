if(CMAKE_SYSTEM_PROCESSOR MATCHES "i?86|x86_64")
	set(SSE2_CXXFLAGS "-msse2")
	set(SSSE3_CXXFLAGS "-mssse3")
	set(PCLMUL_CXXFLAGS "-msse4.1 -mpclmul")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm.*")
	set(NEON_CXXFLAGS "-mfpu=neon")
	set(ACLECRC_CXXFLAGS "-march=armv8-a+crc -fpermissive")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
	set(ACLECRC_CXXFLAGS "-march=armv8-a+crc -fpermissive")
endif()

add_library(regex STATIC
	${CMAKE_SOURCE_DIR}/lib/regex/regex.c
)
target_include_directories(regex PUBLIC
	${CMAKE_SOURCE_DIR}/lib/regex
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

set_source_files_properties(
	${CMAKE_SOURCE_DIR}/lib/yencode/Sse2Decoder.cpp
	PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${SSE2_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_SOURCE_DIR}/lib/yencode/Ssse3Decoder.cpp
	PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${SSSE3_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_SOURCE_DIR}/lib/yencode/PclmulCrc.cpp
	PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${PCLMUL_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_SOURCE_DIR}/lib/yencode/NeonDecoder.cpp
	PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} ${NEON_CXXFLAGS}"
)

set_source_files_properties(
	${CMAKE_SOURCE_DIR}/lib/yencode/AcleCrc.cpp
	PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${ACLECRC_CXXFLAGS}"
)

if(NOT DISABLE_PARCHECK)
	add_library(par2 STATIC
		${CMAKE_SOURCE_DIR}/lib/par2/commandline.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/crc.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/creatorpacket.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/criticalpacket.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/datablock.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/descriptionpacket.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/diskfile.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/filechecksummer.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/galois.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/mainpacket.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/md5.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/par2fileformat.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/par2repairer.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/par2repairersourcefile.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/parheaders.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/recoverypacket.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/reedsolomon.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/verificationhashtable.cpp
		${CMAKE_SOURCE_DIR}/lib/par2/verificationpacket.cpp
	)
	target_include_directories(par2 PUBLIC
		${CMAKE_SOURCE_DIR}/lib/par2
		${CMAKE_SOURCE_DIR}/lib/regex
		${CMAKE_SOURCE_DIR}/daemon/main
		${CMAKE_SOURCE_DIR}/daemon/util
		${INCLUDES}
	)
endif()

set(LIBS ${LIBS} regex yencode)
set(INCLUDES ${INCLUDES} ${CMAKE_SOURCE_DIR}/lib/regex ${CMAKE_SOURCE_DIR}/lib/yencode)

if(NOT DISABLE_PARCHECK)
	set(LIBS ${LIBS} par2)
	set(INCLUDES ${INCLUDES} ${CMAKE_SOURCE_DIR}/lib/par2)
endif()
