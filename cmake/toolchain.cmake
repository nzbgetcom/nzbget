if(DEFINED ENV{CMAKE_SYSTEM_NAME})
	if (NOT CMAKE_SYSTEM_NAME)
		set(CMAKE_SYSTEM_NAME $ENV{CMAKE_SYSTEM_NAME})
	endif()
else()
	if (CMAKE_SYSTEM_NAME)
		set(ENV{CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_NAME})
	else()
		set(CMAKE_SYSTEM_NAME Linux)
	endif()
endif()
set(CMAKE_SYSTEM_PROCESSOR ${ARCH})

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD")
	set(CMAKE_C_COMPILER          clang)
	set(CMAKE_CXX_COMPILER        clang++)
	SET(CMAKE_CXX_COMPILER_TARGET x86_64-pc-freebsd)
else()
	if(NOT COMPILER)
		set(COMPILER "gcc" CACHE STRING "")
	endif()

	if("${COMPILER}" STREQUAL "gcc")
		set(C_COMPILER   "gcc")
		set(CXX_COMPILER "g++")
	else()
		set(C_COMPILER   ${COMPILER})
		set(CXX_COMPILER ${COMPILER}++)
	endif()
	set(CMAKE_AR           ${TOOLCHAIN_PREFIX}-ar)
	set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}-as)
	set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-${C_COMPILER})
	set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-${CXX_COMPILER})
	set(CMAKE_LINKER       ${TOOLCHAIN_PREFIX}-ld)
	set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}-objcopy)
	set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}-ranlib)
	set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}-size)
	set(CMAKE_STRIP        ${TOOLCHAIN_PREFIX}-strip)
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
