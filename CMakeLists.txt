cmake_minimum_required(VERSION 3.16)
project(zcsr_tools LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()

set(ZCSR_ENGINE_INCLUDE "" CACHE PATH "Path to zero-coupling-signal-router/include")
set(ZCSR_RUNTIME_INCLUDE "" CACHE PATH "Path to zcsr_runtime/zcsr_runtime_shell/include")
set(ZCSR_UIM_PAGE_MANAGER_INCLUDE "" CACHE PATH "Path to UIM/zcsr_ui_page_manager/include")

add_library(zcsr_debug_overlay STATIC
    zcsr_debug_overlay/src/overlay.c)
target_include_directories(zcsr_debug_overlay PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/zcsr_debug_overlay/include)
if(ZCSR_ENGINE_INCLUDE)
  target_include_directories(zcsr_debug_overlay PRIVATE ${ZCSR_ENGINE_INCLUDE})
endif()
if(WIN32)
  target_link_libraries(zcsr_debug_overlay PUBLIC user32 gdi32)
endif()

add_subdirectory(zcsr_workspace_2d5)
