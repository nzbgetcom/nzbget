find_package(Boost 1.83 COMPONENTS unit_test_framework REQUIRED)

include_directories(${Boost_INCLUDE_DIR})
add_subdirectory(util)
add_subdirectory(feed)
add_subdirectory(queue)
add_subdirectory(nntp)
add_subdirectory(main)
add_subdirectory(postprocess)
