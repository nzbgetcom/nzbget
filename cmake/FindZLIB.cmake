# FindZLIB.cmake - In-tree module for FetchContent zlibstatic target
# Provides ZLIB_FOUND, ZLIB_INCLUDE_DIRS, ZLIB_LIBRARIES without disk probing

if(TARGET zlibstatic)
    set(ZLIB_FOUND TRUE)
    set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
    set(ZLIB_LIBRARIES zlibstatic)
    set(ZLIB_VERSION ${zlib_VERSION})
    message(STATUS "Found in-tree zlibstatic target")
elseif(TARGET ZLIB::ZLIB)
    set(ZLIB_FOUND TRUE)
    set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
    set(ZLIB_LIBRARIES ZLIB::ZLIB)
    set(ZLIB_VERSION ${zlib_VERSION})
    message(STATUS "Found in-tree ZLIB::ZLIB target")
else()
    # Fallback to system CMake module for cross-compilation / system zlib
    include(${CMAKE_ROOT}/Modules/FindZLIB.cmake)
endif()

# Ensure standard variables are set for downstream packages
if(ZLIB_FOUND AND NOT ZLIB_LIBRARIES)
    if(TARGET zlibstatic)
        set(ZLIB_LIBRARIES zlibstatic)
    elseif(TARGET ZLIB::ZLIB)
        set(ZLIB_LIBRARIES ZLIB::ZLIB)
    endif()
endif()