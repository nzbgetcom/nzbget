# toolchains/macos-llvm.cmake
# macOS toolchain with upstream LLVM Clang + static libc++ (targeting macOS 12+)
# All locations are auto-detected; no hardcoded compiler versions or paths.
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "Minimum OS X deployment version")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# --- Locate upstream LLVM Clang (any version) ------------------------------
if(NOT CMAKE_C_COMPILER)
    set(_LLVM_BIN "")

    # 1) Homebrew default formula ("brew install llvm", any release)
    find_program(NZBGET_BREW brew)
    if(NZBGET_BREW)
        execute_process(
            COMMAND ${NZBGET_BREW} --prefix llvm
            RESULT_VARIABLE _brew_res
            OUTPUT_VARIABLE _brew_prefix
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_brew_res EQUAL 0 AND EXISTS "${_brew_prefix}/bin/clang")
            set(_LLVM_BIN "${_brew_prefix}/bin")
        endif()
    endif()

    # 2) Any versioned keg (llvm@N) under standard Homebrew prefixes
    if(NOT _LLVM_BIN)
        file(GLOB _kegs "/opt/homebrew/opt/llvm@*" "/usr/local/opt/llvm@*")
        set(_best "")
        set(_best_ver -1)
        foreach(_keg IN LISTS _kegs)
            if(EXISTS "${_keg}/bin/clang")
                string(REGEX REPLACE ".*llvm@([0-9]+)$" "\\1" _ver "${_keg}")
                if(_ver MATCHES "^[0-9]+$" AND _ver GREATER _best_ver)
                    set(_best "${_keg}")
                    set(_best_ver "${_ver}")
                endif()
            endif()
        endforeach()
        if(_best)
            set(_LLVM_BIN "${_best}/bin")
        endif()
    endif()

    if(_LLVM_BIN)
        set(CMAKE_C_COMPILER "${_LLVM_BIN}/clang")
        set(CMAKE_CXX_COMPILER "${_LLVM_BIN}/clang++")
    else()
        message(WARNING
            "Upstream LLVM clang not found (brew install llvm); "
            "falling back to the system compiler. Full C++20 on macOS 12 "
            "requires upstream LLVM clang with static libc++.")
    endif()
endif()

# --- C++ standard library -----------------------------------------------------
# Default: a self-built static libc++ (zig-spec, see
# toolchains/build-macos-libcxx.sh): all symbols are hidden
# (-fvisibility=hidden + _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS baked into
# __config_site), so dyld cannot coalesce them with the system libc++ in the
# shared cache, and -Wl,-force_load pulls every archive member in (the same
# thing zig does by prelinking all libc++ objects into a single CRT blob).
# This gives full C++20 (std::format, ...) on a macOS 12 deployment target.
#
# Search order:
#   1) -DNZBGET_LIBCXX_DIR=<dir> (CMake var) or $NZBGET_LIBCXX_DIR (env)
#      with lib/libc++.a and include/c++/v1
#   2) default location build/toolchains/macos12-libcxx (build-macos-libcxx.sh)
#   3) CI locations: /opt/macos12-libcxx (tarball unpacked into /opt),
#      /tmp/macos12-libcxx
#   4) none found -> system libc++ (dynamic; std::format unavailable on macOS 12)
set(NZBGET_STATIC_LIBCXX "")
foreach(_libcxx_candidate IN ITEMS "${NZBGET_LIBCXX_DIR}" "$ENV{NZBGET_LIBCXX_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/../build/toolchains/macos12-libcxx"
        "/opt/macos12-libcxx"
        "/tmp/macos12-libcxx")
    if(_libcxx_candidate
            AND EXISTS "${_libcxx_candidate}/lib/libc++.a"
            AND IS_DIRECTORY "${_libcxx_candidate}/include/c++/v1")
        set(NZBGET_STATIC_LIBCXX "${_libcxx_candidate}")
        break()
    endif()
endforeach()
unset(_libcxx_candidate)

if(NZBGET_STATIC_LIBCXX)
    # Consumer compile flags: use our headers, no Apple availability markup.
    # The visibility annotations switch lives in the installed __config_site,
    # so library and consumer instantiations always agree.
    set(_LIBCXX_CXX_FLAGS "-nostdinc++ -isystem ${NZBGET_STATIC_LIBCXX}/include/c++/v1 -D_LIBCPP_DISABLE_AVAILABILITY")
    set(CMAKE_CXX_FLAGS_INIT "${_LIBCXX_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_LIBCXX_CXX_FLAGS}")

    set(_LIBCXX_ABI "")
    if(EXISTS "${NZBGET_STATIC_LIBCXX}/lib/libc++abi.a")
        set(_LIBCXX_ABI "${NZBGET_STATIC_LIBCXX}/lib/libc++abi.a")
    endif()

    # Link order matches zig (MachO.zig): libc++abi.a first, then libc++.a;
    # -force_load on libc++.a equals zig prelinking every object into one blob,
    # which keeps lazy archive extraction from dropping weak vtable symbols.
    set(_LIBCXX_LINK_FLAGS "-nostdlib++ -Wl,-force_load,${NZBGET_STATIC_LIBCXX}/lib/libc++.a ${_LIBCXX_ABI}")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "${_LIBCXX_LINK_FLAGS}")

    message(STATUS "NZBGet: static libc++ from ${NZBGET_STATIC_LIBCXX}")
else()
    message(STATUS
        "NZBGet: static libc++ not found - using the system libc++ (dynamic); "
        "run toolchains/build-macos-libcxx.sh or set NZBGET_LIBCXX_DIR for "
        "full C++20 (std::format) on macOS 12")
endif()
