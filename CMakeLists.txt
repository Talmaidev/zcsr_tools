cmake_minimum_required(VERSION 3.16)
project(zcsr_tools LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()

enable_testing()

# Tools are a separate repo; point these at the real repos.
# Defaults use the engine repo root and the sibling runtime/UIM repos.
set(ZCSR_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../zero-coupling-signal-router" CACHE PATH "zcsr engine repo")
set(ZCSR_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../zcsr_runtime" CACHE PATH "zcsr runtime repo")
set(ZCSR_UIM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../UIM" CACHE PATH "UIM repo")
set(ZCSR_ENGINE_INCLUDE "${ZCSR_ENGINE_DIR}/include" CACHE PATH "Path to zero-coupling-signal-router/include")
set(ZCSR_RUNTIME_INCLUDE "${ZCSR_RUNTIME_DIR}/zcsr_runtime_shell/include" CACHE PATH "Path to zcsr_runtime/zcsr_runtime_shell/include")
set(ZCSR_UIM_PAGE_MANAGER_INCLUDE "${ZCSR_UIM_DIR}/zcsr_ui_page_manager/include" CACHE PATH "Path to UIM/zcsr_ui_page_manager/include")

# 两层边界：
# common/       通用必备功能，任何 2D/非 2D 游戏都能直接复用。
# tower_defense/ 塔防大类必备功能，只有塔防语义，不含 PVZ 数值与美术。
add_subdirectory(common)
add_subdirectory(tower_defense)

add_test(NAME layer_boundary
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_layer_boundary.cmake")
