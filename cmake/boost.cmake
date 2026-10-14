include(FetchContent)

set(BOOST_VERSION "1.92.0")
set(BOOST_SHA256 "e2a814b3a158ab482c7a3d330f8bf5a7a8423d258a4e2fb396996e00fcee2111")
set(BOOST_URL "https://github.com/boostorg/boost/releases/download/boost-${BOOST_VERSION}/boost-${BOOST_VERSION}-b2-nodocs.tar.gz")

if(NOT BUILD_DEPS_FROM_SOURCE)
	# Local development: try system Boost first
	find_package(Boost QUIET COMPONENTS json unit_test_framework)
	if(Boost_FOUND)
		message(STATUS "Using system Boost ${Boost_VERSION} (found via find_package)")
		list(APPEND EXTERNAL_DEPS Boost::headers Boost::json Boost::unit_test_framework)
		set(BOOST_FROM_SYSTEM 1)
	endif()
endif()

if(NOT BOOST_FROM_SYSTEM)
	message(STATUS "Building Boost ${BOOST_VERSION} via FetchContent (header-only)")

	# EXCLUDE_FROM_ALL: keep Boost install rules out of the nzbget install
	FetchContent_Declare(boost
		URL ${BOOST_URL}
		URL_HASH SHA256=${BOOST_SHA256}
		EXCLUDE_FROM_ALL
	)

	FetchContent_MakeAvailable(boost)

	add_library(Boost::headers INTERFACE IMPORTED GLOBAL)
	target_include_directories(Boost::headers INTERFACE ${boost_SOURCE_DIR})
	target_compile_definitions(Boost::headers INTERFACE BOOST_ALL_NO_LIB)

	add_library(Boost::boost INTERFACE IMPORTED GLOBAL)
	target_link_libraries(Boost::boost INTERFACE Boost::headers)

	add_library(Boost::unit_test_framework INTERFACE IMPORTED GLOBAL)
	target_link_libraries(Boost::unit_test_framework INTERFACE Boost::headers)

	add_library(Boost::json INTERFACE IMPORTED GLOBAL)
	target_link_libraries(Boost::json INTERFACE Boost::headers)

	target_compile_definitions(libnzbget PUBLIC NZBGET_BOOST_JSON_SOURCE)
endif()

# Disable Boost auto-linking; dependencies are linked explicitly via CMake targets.
add_compile_definitions(BOOST_ALL_NO_LIB)

target_link_libraries(libnzbget PUBLIC Boost::json)
