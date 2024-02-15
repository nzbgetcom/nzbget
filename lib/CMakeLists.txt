add_library(Regex STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/regex/regex.c
)
target_include_directories(Regex PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/regex
)

add_library(Sse2Decoder STATIC ${CMAKE_CURRENT_SOURCE_DIR}/yencode/Sse2Decoder.cpp)
target_compile_options(Sse2Decoder PRIVATE ${SSE2_CXXFLAGS})
target_include_directories(Sse2Decoder PRIVATE 
	${CMAKE_SOURCE_DIR}/daemon/main 
	${LIBXML2_INCLUDE_DIR} 
	${Iconv_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/regex
)
add_library(Ssse3Decoder STATIC ${CMAKE_CURRENT_SOURCE_DIR}/yencode/Ssse3Decoder.cpp)
target_compile_options(Ssse3Decoder PRIVATE ${CMAKE_CXX_FLAGS_RELEASE} ${SSSE3_CXXFLAGS})
target_include_directories(Ssse3Decoder PRIVATE ${CMAKE_SOURCE_DIR}/daemon/main 
	${CMAKE_SOURCE_DIR}/daemon/main 
	${LIBXML2_INCLUDE_DIR} 
	${Iconv_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/regex
)
add_library(PclmulCrc STATIC ${CMAKE_CURRENT_SOURCE_DIR}/yencode/PclmulCrc.cpp)
target_compile_options(PclmulCrc PRIVATE ${CMAKE_CXX_FLAGS_RELEASE} ${PCLMUL_CXXFLAGS})
target_include_directories(PclmulCrc PRIVATE ${CMAKE_SOURCE_DIR}/daemon/main 
	${CMAKE_SOURCE_DIR}/daemon/main 
	${LIBXML2_INCLUDE_DIR} 
	${Iconv_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/regex
)
add_library(NeonDecoder STATIC ${CMAKE_CURRENT_SOURCE_DIR}/yencode/NeonDecoder.cpp)
target_compile_options(NeonDecoder PRIVATE ${CMAKE_CXX_FLAGS_RELEASE} ${NEON_CXXFLAGS})
target_include_directories(NeonDecoder PRIVATE ${CMAKE_SOURCE_DIR}/daemon/main 
	${CMAKE_SOURCE_DIR}/daemon/main 
	${LIBXML2_INCLUDE_DIR} 
	${Iconv_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/regex
)
add_library(AcleCrc STATIC ${CMAKE_CURRENT_SOURCE_DIR}/yencode/AcleCrc.cpp)
target_compile_options(AcleCrc PRIVATE ${CMAKE_CXX_FLAGS_RELEASE} ${ACLECRC_CXXFLAGS})
target_include_directories(AcleCrc PRIVATE 
	${CMAKE_SOURCE_DIR}/daemon/main 
	${LIBXML2_INCLUDE_DIR} 
	${Iconv_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/regex
)

add_library(Yencode STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/SimdInit.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/SimdDecoder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/ScalarDecoder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/yencode/SliceCrc.cpp
)
target_include_directories(Yencode PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/yencode
	${CMAKE_CURRENT_SOURCE_DIR}/regex
	${CMAKE_SOURCE_DIR}/daemon/main
	${LIBXML2_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIR}
	${OPENSSL_INCLUDE_DIR}
)
target_link_libraries(Yencode PRIVATE Sse2Decoder Ssse3Decoder PclmulCrc NeonDecoder AcleCrc)

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
	${OPENSSL_INCLUDE_DIR}
)
