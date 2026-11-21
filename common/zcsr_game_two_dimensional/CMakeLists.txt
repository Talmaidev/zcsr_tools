cmake_minimum_required(VERSION 3.16)

add_library(zcsr_game_two_dimensional STATIC
    src/game_two_dimensional.c)
target_include_directories(zcsr_game_two_dimensional PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(game_two_dimensional_tests
    tests/test_game_two_dimensional.c)
target_link_libraries(game_two_dimensional_tests PRIVATE
    zcsr_game_two_dimensional)
add_test(NAME game_two_dimensional COMMAND game_two_dimensional_tests)
