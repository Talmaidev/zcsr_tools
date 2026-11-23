cmake_minimum_required(VERSION 3.16)

add_library(zcsr_spatial_grid STATIC
    src/spatial_grid.c)
target_include_directories(zcsr_spatial_grid PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_executable(spatial_grid_tests
    tests/test_spatial_grid.c)
target_link_libraries(spatial_grid_tests PRIVATE
    zcsr_spatial_grid)
add_test(NAME spatial_grid COMMAND spatial_grid_tests)
