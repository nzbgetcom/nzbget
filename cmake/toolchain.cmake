# Without that flag CMake is not able to pass test compilation check
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_AR           ${TOOLCHAIN_PATH}${TARGET_TRIPLE}-ar)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PATH}${TARGET_TRIPLE}-as)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PATH}${TARGET_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}${TARGET_TRIPLE}-g++)
set(CMAKE_LINKER       ${TOOLCHAIN_PATH}${TARGET_TRIPLE}-ld)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PATH}${TARGET_TRIPLE}-objcopy)
set(CMAKE_RANLIB       ${TOOLCHAIN_PATH}${TARGET_TRIPLE}-ranlib)
set(CMAKE_SIZE         ${TOOLCHAIN_PATH}${TARGET_TRIPLE}-size)
set(CMAKE_STRIP        ${TOOLCHAIN_PATH}${TARGET_TRIPLE}-strip)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
