cmake_minimum_required(VERSION 3.16)
project(zcsr_tools LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()

# Tools are a separate repo; point these at the real repos.
# Defaults use the stable merged engine main and the sibling runtime/UIM repos.
set(ZCSR_ENGINE_DIR "C:/Users/chen/Documents/GitHub/_worktrees/zcsr-main" CACHE PATH "zcsr engine repo (merged main)")
set(ZCSR_RUNTIME_DIR "C:/Users/chen/Documents/GitHub/zcsr_runtime" CACHE PATH "zcsr runtime repo")
set(ZCSR_UIM_DIR "C:/Users/chen/Documents/GitHub/UIM" CACHE PATH "UIM repo")
set(ZCSR_ENGINE_INCLUDE "${ZCSR_ENGINE_DIR}/include" CACHE PATH "Path to zero-coupling-signal-router/include")
set(ZCSR_RUNTIME_INCLUDE "${ZCSR_RUNTIME_DIR}/zcsr_runtime_shell/include" CACHE PATH "Path to zcsr_runtime/zcsr_runtime_shell/include")
set(ZCSR_UIM_PAGE_MANAGER_INCLUDE "${ZCSR_UIM_DIR}/zcsr_ui_page_manager/include" CACHE PATH "Path to UIM/zcsr_ui_page_manager/include")

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
