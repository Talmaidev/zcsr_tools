cmake_minimum_required(VERSION 3.16)

add_library(zcsr_debug_draw STATIC src/debug_draw.c)
target_include_directories(zcsr_debug_draw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(test_debug_draw tests/test_debug_draw.c)
target_link_libraries(test_debug_draw PRIVATE zcsr_debug_draw)
add_test(NAME debug_draw COMMAND test_debug_draw)
