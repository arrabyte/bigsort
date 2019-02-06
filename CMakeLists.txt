cmake_minimum_required(VERSION 3.0.0)
project(bigsort VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckCXXSymbolExists)

set (StdFilesystem_LIBRARY_NAME stdc++fs)

find_package (Seastar REQUIRED)

include(CTest)
enable_testing()
add_subdirectory(tests)

add_executable(bigsort bigsort.cc sort_strategies.cc block.cc)
target_link_libraries (${PROJECT_NAME} PRIVATE Seastar::seastar stdc++fs)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
