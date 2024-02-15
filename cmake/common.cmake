if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
		set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Weverything" CACHE STRING "" FORCE)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall -Wextra" CACHE STRING "" FORCE)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /MP /W4 /DDEBUG /D_DEBUG /wd4800 /wd4267" CACHE STRING "" FORCE)
		set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} winmm.lib Dbghelp.lib libcpmtd.lib" CACHE STRING "" FORCE)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug" CACHE STRING "" FORCE)
	endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
		set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -Weverything" CACHE STRING "" FORCE)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -Wall -Wextra" CACHE STRING "" FORCE)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MP /W4 /DNDEBUG /wd4800 /wd4267" CACHE STRING "" FORCE)
		set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} winmm.lib" CACHE STRING "" FORCE)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)
	endif()
endif()

if(NOT MSVC)
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "i?86|x86_64")
		set(SSE2_CXXFLAGS "-msse2")
		set(SSSE3_CXXFLAGS "-mssse3")
		set(PCLMUL_CXXFLAGS "-msse4.1 -mpclmul")
	elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm*")
		set(NEON_CXXFLAGS "-mfpu=neon")
		set(ACLECRC_CXXFLAGS "-march=armv8-a+crc -fpermissive")
	elseif (${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
		set(ACLECRC_CXXFLAGS "-march=armv8-a+crc -fpermissive")
	endif()
endif()

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)

include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(${CMAKE_SOURCE_DIR}/daemon)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib)


target_link_libraries(${PACKAGE} PRIVATE
	Yencode 
	Regex 
	Threads::Threads 
	Boost::json 
	ZLIB::ZLIB 
	LibXml2::LibXml2
)
target_include_directories(${PACKAGE} PRIVATE
	${CMAKE_SOURCE_DIR}/daemon/connect
	${CMAKE_SOURCE_DIR}/daemon/extension
	${CMAKE_SOURCE_DIR}/daemon/feed
	${CMAKE_SOURCE_DIR}/daemon/frontend
	${CMAKE_SOURCE_DIR}/daemon/main
	${CMAKE_SOURCE_DIR}/daemon/nntp
	${CMAKE_SOURCE_DIR}/daemon/nserv
	${CMAKE_SOURCE_DIR}/daemon/postprocess
	${CMAKE_SOURCE_DIR}/daemon/queue
	${CMAKE_SOURCE_DIR}/daemon/remote
	${CMAKE_SOURCE_DIR}/daemon/util
	${CMAKE_SOURCE_DIR}/lib/regex
	${CMAKE_SOURCE_DIR}/lib/yencode
	${Boost_INCLUDE_DIR}
	${LIBXML2_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIRS}
)
