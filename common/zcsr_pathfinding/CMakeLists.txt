cmake_minimum_required(VERSION 3.16)

add_library(zcsr_pathfinding STATIC
    src/pathfinding.c)
target_include_directories(zcsr_pathfinding PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_executable(pathfinding_tests
    tests/test_pathfinding.c)
target_link_libraries(pathfinding_tests PRIVATE
    zcsr_pathfinding)
add_test(NAME pathfinding COMMAND pathfinding_tests)
