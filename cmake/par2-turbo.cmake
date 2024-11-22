set(ROOT ${CMAKE_BINARY_DIR}/par2-turbo)

add_compile_definitions(HAVE_CONFIG_H PARPAR_ENABLE_HASHER_MD5CRC)
ExternalProject_add(
	par2-turbo
	PREFIX par2-turbo
	GIT_REPOSITORY  https://github.com/nzbgetcom/par2cmdline-turbo.git
	GIT_TAG         v1.1.1-nzbget
	TLS_VERIFY	    TRUE
	GIT_SHALLOW     TRUE
	GIT_PROGRESS	TRUE
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	CMAKE_ARGS  -DBUILD_TOOL=OFF 
				-DBUILD_LIB=ON 
				-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
				-DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
				-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
				-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
				-DCMAKE_SYSROOT=${CMAKE_SYSROOT}
				-DCMAKE_CXX_FLAGS=-I${CMAKE_SYSROOT}/usr/include/c++/v1
	INSTALL_COMMAND ""
)

if(WIN32) 
	set(LIBS ${LIBS} 
		${ROOT}/src/par2-turbo-build/${CMAKE_BUILD_TYPE}/par2-turbo.lib
		${ROOT}/src/par2-turbo-build/${CMAKE_BUILD_TYPE}/gf16.lib
		${ROOT}/src/par2-turbo-build/${CMAKE_BUILD_TYPE}/hasher.lib
	)
else()
	set(LIBS ${LIBS} 
		${ROOT}/src/par2-turbo-build/libpar2-turbo.a
		${ROOT}/src/par2-turbo-build/libgf16.a
		${ROOT}/src/par2-turbo-build/libhasher.a
	)
endif()

set(INCLUDES ${INCLUDES} ${ROOT}/src/par2-turbo/include)
