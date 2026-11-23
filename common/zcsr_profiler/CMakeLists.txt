cmake_minimum_required(VERSION 3.16)

add_library(zcsr_profiler STATIC src/profiler.c)
target_include_directories(zcsr_profiler PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_executable(test_profiler tests/test_profiler.c)
target_link_libraries(test_profiler PRIVATE zcsr_profiler)
add_test(NAME profiler COMMAND test_profiler)
