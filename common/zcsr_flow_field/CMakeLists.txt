cmake_minimum_required(VERSION 3.16)

add_library(zcsr_flow_field STATIC
    src/flow_field.c)
target_include_directories(zcsr_flow_field PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_executable(flow_field_tests
    tests/test_flow_field.c)
target_link_libraries(flow_field_tests PRIVATE
    zcsr_flow_field)
add_test(NAME flow_field COMMAND flow_field_tests)
