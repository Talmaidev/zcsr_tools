cmake_minimum_required(VERSION 3.16)

add_library(zcsr_bench STATIC src/bench.c)
target_include_directories(zcsr_bench PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_executable(test_bench tests/test_bench.c)
target_link_libraries(test_bench PRIVATE zcsr_bench)
add_test(NAME bench COMMAND test_bench)
