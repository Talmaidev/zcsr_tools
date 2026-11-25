cmake_minimum_required(VERSION 3.16)

add_library(zcsr_rng STATIC
    src/rng.c)
target_include_directories(zcsr_rng PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_executable(rng_tests
    tests/test_rng.c)
target_link_libraries(rng_tests PRIVATE
    zcsr_rng)
add_test(NAME rng COMMAND rng_tests)
