cmake_minimum_required(VERSION 3.16)

# --- Hard guard: the build tree must live OUTSIDE the source tree ------------
# Same layout guard as zero-coupling-signal-router. CMake 3.16 compatible
# (file(TO_CMAKE_PATH), no cmake_path): in-source / in-repo build directories
# are refused at configure time so the source tree stays free of build output.
if(CMAKE_BINARY_DIR)
  file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" _zcsr_guard_src)
  file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}" _zcsr_guard_bin)
  if(WIN32)
    string(TOLOWER "${_zcsr_guard_src}" _zcsr_guard_src_cmp)
    string(TOLOWER "${_zcsr_guard_bin}" _zcsr_guard_bin_cmp)
  else()
    set(_zcsr_guard_src_cmp "${_zcsr_guard_src}")
    set(_zcsr_guard_bin_cmp "${_zcsr_guard_bin}")
  endif()
  string(LENGTH "${_zcsr_guard_src_cmp}" _zcsr_guard_src_len)
  string(SUBSTRING "${_zcsr_guard_bin_cmp}" 0 ${_zcsr_guard_src_len}
         _zcsr_guard_bin_head)
  set(_zcsr_guard_in_repo FALSE)
  if(_zcsr_guard_bin_cmp STREQUAL _zcsr_guard_src_cmp)
    set(_zcsr_guard_in_repo TRUE)
  elseif(_zcsr_guard_bin_head STREQUAL _zcsr_guard_src_cmp)
    string(SUBSTRING "${_zcsr_guard_bin_cmp}" ${_zcsr_guard_src_len} 1
           _zcsr_guard_sep)
    if(_zcsr_guard_sep STREQUAL "/" OR _zcsr_guard_sep STREQUAL "\\")
      set(_zcsr_guard_in_repo TRUE)
    endif()
  endif()
  if(_zcsr_guard_in_repo)
    message(FATAL_ERROR
      "In-repo build directory refused by ZCSR build-layout guard.\n"
      "  CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}\n"
      "  CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}\n"
      "\n"
      "Build trees must live outside the source tree: this repository (like\n"
      "every repository under C:\\Users\\chen\\Documents\\GitHub) must stay clean.\n"
      "Put the build tree at one of these instead:\n"
      "  Windows: C:\\Users\\chen\\Documents\\build\\zcsr\\zcsr_tools\\<preset>\n"
      "  Linux:   /home/chen/zcsr-builds/zcsr_tools/<preset>\n"
      "\n"
      "Example (Windows, Visual Studio generator):\n"
      "  cmake -S . -B \"C:\\Users\\chen\\Documents\\build\\zcsr\\zcsr_tools\\build_main\" -G \"Visual Studio 18 2026\"\n"
      "Example (Linux):\n"
      "  cmake -S . -B /home/chen/zcsr-builds/zcsr_tools/linux-gcc\n")
  endif()
  unset(_zcsr_guard_src)
  unset(_zcsr_guard_bin)
  unset(_zcsr_guard_src_cmp)
  unset(_zcsr_guard_bin_cmp)
  unset(_zcsr_guard_src_len)
  unset(_zcsr_guard_bin_head)
  unset(_zcsr_guard_sep)
  unset(_zcsr_guard_in_repo)
endif()

project(zcsr_tools VERSION 1.0.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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

# Tools are a separate repo. They consume the engine through its CMake
# targets, never by compiling engine module .c files into tool executables.
# When the engine is already provided by a parent build, reuse that target.
if(NOT TARGET zcsr_platform)
  add_subdirectory(${ZCSR_ENGINE_DIR} "${CMAKE_CURRENT_BINARY_DIR}/zcsr_engine")
endif()

# 两层边界：
# common/       通用必备功能，任何 2D/非 2D 游戏都能直接复用。
# tower_defense/ 塔防大类必备功能，只有塔防语义，不含 PVZ 数值与美术。
add_subdirectory(common)
add_subdirectory(tower_defense)

# --- Install / export package -------------------------------------------------
set(ZCSR_TOOL_TARGETS
  zcsr_bench
  zcsr_debug_draw
  zcsr_debug_overlay
  zcsr_game_two_dimensional
  zcsr_pathfinding
  zcsr_rng
  zcsr_flow_field
  zcsr_spatial_grid
  zcsr_profiler
  zcsr_tower_defense)
# zcsr_workspace_2d5 is a fixed 2.5D reference/demo that consumes the runtime
# shell. It is built and tested here, but it is not a self-contained installed
# package target; exporting it without Runtime/UIM targets leaves unresolved
# symbols for downstream consumers.
install(TARGETS ${ZCSR_TOOL_TARGETS}
  EXPORT zcsr_toolsTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
file(GLOB ZCSR_TOOL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/common/*/include
  ${CMAKE_CURRENT_SOURCE_DIR}/tower_defense/*/include)
list(FILTER ZCSR_TOOL_INCLUDE_DIRS EXCLUDE REGEX "zcsr_workspace_2d5")
foreach(dir ${ZCSR_TOOL_INCLUDE_DIRS})
  install(DIRECTORY "${dir}/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endforeach()
install(EXPORT zcsr_toolsTargets
  FILE zcsr_toolsTargets.cmake
  NAMESPACE zcsr::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zcsr_tools)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/zcsr_toolsConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/zcsr_toolsConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zcsr_tools)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/zcsr_toolsConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/zcsr_toolsConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/zcsr_toolsConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zcsr_tools)

add_test(NAME layer_boundary
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_layer_boundary.cmake")
