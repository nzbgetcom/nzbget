include(ExternalProject)

set(NCURSES_VERSION "6.5")
set(NCURSES_SHA256 "136d91bc269a9a5785e5f9e980bc76ab57428f604ce3e5a5a90cebc767971cc6")
set(NCURSES_URL "https://ftp.gnu.org/gnu/ncurses/ncurses-${NCURSES_VERSION}.tar.gz")

# Version-tagged directory paths prevent stale binary reuse across version bumps
set(NCURSES_INSTALL_DIR "${CMAKE_BINARY_DIR}/deps/ncurses-${NCURSES_VERSION}")
set(NCURSES_LIBRARY     "${NCURSES_INSTALL_DIR}/lib/libncursesw.a")

if(EXISTS "${NCURSES_LIBRARY}" AND EXISTS "${NCURSES_INSTALL_DIR}/include/ncurses.h")
	message(STATUS "Using prebuilt ncurses ${NCURSES_VERSION} from cache: ${NCURSES_INSTALL_DIR}")
else()
	message(STATUS "Building ncurses ${NCURSES_VERSION} via ExternalProject")

	find_program(TIC_EXECUTABLE tic
		HINTS
			/opt/homebrew/opt/ncurses/bin
			/usr/local/opt/ncurses/bin
	)
	find_program(INFOCMP_EXECUTABLE infocmp
		HINTS
			/opt/homebrew/opt/ncurses/bin
			/usr/local/opt/ncurses/bin
	)
	if(NOT TIC_EXECUTABLE)
		message(FATAL_ERROR "Building ncurses with built-in terminal fallbacks requires 'tic' on the build host (package ncurses-bin / ncurses)")
	endif()

	if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
		set(NCURSES_JOBS "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
	else()
		include(ProcessorCount)
		ProcessorCount(NCORES)
		if(NCORES EQUAL 0)
			set(NCORES 2)
		endif()
		set(NCURSES_JOBS "${NCORES}")
	endif()

	# Android has no terminfo database: compile in fallbacks for common terminals
	set(NCURSES_FALLBACKS "xterm,xterm-color,xterm-256color,xterm-16color,vt100,vt200,linux,ansi,screen,screen-256color,tmux,tmux-256color")

	file(MAKE_DIRECTORY "${NCURSES_INSTALL_DIR}/include")
	file(MAKE_DIRECTORY "${NCURSES_INSTALL_DIR}/lib")

	ExternalProject_Add(ncurses_build
		PREFIX            "${CMAKE_BINARY_DIR}/deps/ncurses-${NCURSES_VERSION}-ep"
		URL               ${NCURSES_URL}
		URL_HASH          SHA256=${NCURSES_SHA256}
		UPDATE_COMMAND    ""
		CONFIGURE_COMMAND
			${CMAKE_COMMAND} -E env
				"CC=${CMAKE_C_COMPILER}"
				"CPP=${CMAKE_C_COMPILER} -E"
				"AR=${CMAKE_AR}"
				"RANLIB=${CMAKE_RANLIB}"
				"CFLAGS=${CMAKE_C_FLAGS} -O2 -ffunction-sections -fdata-sections"
				"CPPFLAGS=${CMAKE_C_FLAGS}"
				"LDFLAGS=--target=${CMAKE_C_COMPILER_TARGET} --sysroot=${CMAKE_SYSROOT} -fuse-ld=lld -rtlib=compiler-rt --unwindlib=none"
			<SOURCE_DIR>/configure
				--host=${ANDROID_TARGET_TRIPLE}
				--prefix=${NCURSES_INSTALL_DIR}
				--with-tic-path=${TIC_EXECUTABLE}
				--with-infocmp-path=${INFOCMP_EXECUTABLE}
				--enable-widec
				--enable-overwrite
				--enable-ext-colors
				--enable-pc-files
				--enable-const
				--enable-echo
				--disable-stripping
				--disable-big-core
				--disable-rpath
				--disable-rpath-hack
				--without-shared
				--with-normal
				--without-debug
				--without-profile
				--without-cxx
				--without-cxx-binding
				--without-ada
				--without-tests
				--without-manpages
				--without-gpm
				"--with-fallbacks=${NCURSES_FALLBACKS}"
		BUILD_COMMAND     make -j${NCURSES_JOBS} all
		INSTALL_COMMAND   sh -c "make -C include install.includes && make -C ncurses install.libs"
		BUILD_BYPRODUCTS  "${NCURSES_LIBRARY}"
	)
endif()

add_library(Curses::ncursesw STATIC IMPORTED GLOBAL)
set_target_properties(Curses::ncursesw PROPERTIES
	IMPORTED_LOCATION "${NCURSES_LIBRARY}"
	INTERFACE_INCLUDE_DIRECTORIES "${NCURSES_INSTALL_DIR}/include"
)

if(TARGET ncurses_build)
	add_dependencies(Curses::ncursesw ncurses_build)
endif()

list(APPEND EXTERNAL_DEPS Curses::ncursesw)
