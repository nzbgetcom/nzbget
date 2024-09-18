if(APPLE)
	# On macOS Cmake, when cross-compiling, sometimes CMAKE_SYSTEM_PROCESSOR wrongfully stays
	# the same as CMAKE_HOST_SYSTEM_PROCESSOR regardless the target CPU.
	# The manual call to set(CMAKE_SYSTEM_PROCESSOR) has to be set after the project() call.
	# because project() might reset CMAKE_SYSTEM_PROCESSOR back to the value of CMAKE_HOST_SYSTEM_PROCESSOR.
	# Check if CMAKE_SYSTEM_PROCESSOR is not equal to CMAKE_OSX_ARCHITECTURES
	if(NOT CMAKE_OSX_ARCHITECTURES STREQUAL "")
		if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL CMAKE_OSX_ARCHITECTURES)
			# Split CMAKE_OSX_ARCHITECTURES into a list
			string(REPLACE ";" " " ARCH_LIST ${CMAKE_OSX_ARCHITECTURES})
			separate_arguments(ARCH_LIST UNIX_COMMAND ${ARCH_LIST})
			# Count the number of architectures
			list(LENGTH ARCH_LIST ARCH_COUNT)
			# Ensure that exactly one architecture is specified
			if(NOT ARCH_COUNT EQUAL 1)
				message(FATAL_ERROR "CMAKE_OSX_ARCHITECTURES must have exactly one value. Current value: ${CMAKE_OSX_ARCHITECTURES}")
			endif()
			set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_OSX_ARCHITECTURES})
			message(STATUS "CMAKE_SYSTEM_PROCESSOR is manually set to ${CMAKE_SYSTEM_PROCESSOR}")
		endif()
	endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        add_compile_options(-Weverything)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wall -Wextra)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/Zi /MTd /MP /EHs /W4)
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/MT /Oi /MP /EHs /GR- /RTC-)
    else()
        add_compile_options(-fno-rtti -fstack-protector-strong -Wno-unused-function)
    endif()
endif()

include(FetchContent)
