project(unit_test)

add_definitions(-DTEST_PATTERN_DIR="/tmp")
add_definitions(-DBoost_TEST_DYN_LINK=1)
add_definitions(-DSEASTAR_HAVE_GCC6_CONCEPTS=1)

set(CMAKE_C_FLAGS "-DBoost_TEST_DYN_LINK")


add_executable(${PROJECT_NAME} unit_test.cc ../sort_strategies.cc ../block.cc)

target_link_libraries (${PROJECT_NAME}
    Seastar::seastar
    Boost::boost
    Boost::program_options
    Boost::thread
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    seastar_testing
)

