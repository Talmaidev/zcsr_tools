cmake_minimum_required(VERSION 3.16)

add_library(zcsr_tower_defense STATIC src/tower_defense.c)
target_include_directories(zcsr_tower_defense PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(test_tower_defense tests/test_tower_defense.c)
target_link_libraries(test_tower_defense PRIVATE zcsr_tower_defense)
add_test(NAME tower_defense COMMAND test_tower_defense)
