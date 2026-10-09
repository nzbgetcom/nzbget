set(CMAKE_SYSTEM_NAME Darwin)
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
	if("x86_64" IN_LIST CMAKE_OSX_ARCHITECTURES OR (NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64"))
		set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14 CACHE STRING "Minimum OS X deployment version")
	else()
		set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE STRING "Minimum OS X deployment version")
	endif()
endif()
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT CMAKE_C_COMPILER)
	set(_LLVM_BIN "")

	# 1) Specific versioned keg matching pinned LLVM (llvm@19)
	find_program(NZBGET_BREW brew)
	if(NZBGET_BREW)
		execute_process(
			COMMAND ${NZBGET_BREW} --prefix llvm@19
			RESULT_VARIABLE _brew_res
			OUTPUT_VARIABLE _brew_prefix
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
		if(_brew_res EQUAL 0 AND EXISTS "${_brew_prefix}/bin/clang")
			set(_LLVM_BIN "${_brew_prefix}/bin")
		endif()
	endif()

	if(NOT _LLVM_BIN)
		if(EXISTS "/opt/homebrew/opt/llvm@19/bin/clang")
			set(_LLVM_BIN "/opt/homebrew/opt/llvm@19/bin")
		elseif(EXISTS "/usr/local/opt/llvm@19/bin/clang")
			set(_LLVM_BIN "/usr/local/opt/llvm@19/bin")
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

	# 3) Homebrew default unversioned formula ("brew install llvm")
	if(NOT _LLVM_BIN AND NZBGET_BREW)
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

	if(_LLVM_BIN)
		execute_process(
			COMMAND "${_LLVM_BIN}/clang" --version
			OUTPUT_VARIABLE _clang_ver_out
			ERROR_QUIET)
		string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" _clang_ver "${_clang_ver_out}")
		message(STATUS "Discovered LLVM Clang ${_clang_ver} at ${_LLVM_BIN}")
		if(DEFINED ENV{CI} OR DEFINED ENV{GITHUB_ACTIONS})
			if(NOT _clang_ver MATCHES "^19\\.")
				message(FATAL_ERROR
					"CI requires LLVM 19.x to match prebuilt static libc++ (got ${_clang_ver} at ${_LLVM_BIN})")
			endif()
		endif()
		set(CMAKE_C_COMPILER "${_LLVM_BIN}/clang")
		set(CMAKE_CXX_COMPILER "${_LLVM_BIN}/clang++")
	else()
		if(DEFINED ENV{CI} OR DEFINED ENV{GITHUB_ACTIONS})
			message(FATAL_ERROR
				"Upstream LLVM clang not found in CI environment. Expected LLVM 19.")
		else()
			message(WARNING
				"Upstream LLVM clang not found (brew install llvm); "
				"falling back to the system compiler. Full C++20 on macOS 12 "
				"requires upstream LLVM clang with static libc++.")
		endif()
	endif()
endif()

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
		"${CMAKE_CURRENT_LIST_DIR}/../build/toolchains/macos-libcxx"
		"${CMAKE_CURRENT_LIST_DIR}/../build/toolchains/macos12-libcxx"
		"/opt/macos-libcxx"
		"/opt/macos12-libcxx"
		"/tmp/macos-libcxx"
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
	if(DEFINED ENV{CI} OR DEFINED ENV{GITHUB_ACTIONS})
		message(FATAL_ERROR
			"NZBGet: static libc++ not found in CI environment. Refusing to build against dynamic system libc++.")
	else()
		message(STATUS
			"NZBGet: static libc++ not found - using the system libc++ (dynamic)")
	endif()
endif()
