project(unit_test)

add_definitions( -DTEST_PATTERN_DIR="/tmp" )

add_executable(${PROJECT_NAME} unit_test.cc ../sort_strategies.cc ../block.cc)

target_link_libraries (${PROJECT_NAME}
    Seastar::seastar
    Boost::boost
    Boost::program_options
    Boost::thread
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
)

#add_test(test_internal_sort ${PROJECT_NAME} unit_test)
#add_test(test_external_sort ${PROJECT_NAME} unit_test)
#add_test(test_read_blocks_from_file ${PROJECT_NAME} unit_test)


