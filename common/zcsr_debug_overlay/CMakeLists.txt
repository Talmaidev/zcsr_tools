cmake_minimum_required(VERSION 3.16)

add_library(zcsr_debug_overlay STATIC src/overlay.c)
target_include_directories(zcsr_debug_overlay PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(ZCSR_ENGINE_INCLUDE)
  target_include_directories(zcsr_debug_overlay PUBLIC $<BUILD_INTERFACE:${ZCSR_ENGINE_INCLUDE}>)
endif()
# overlay only needs the public zcsr/platform.h surface; consume the engine
# target instead of compiling platform_common.c/platform_win32.c into tools.
target_link_libraries(zcsr_debug_overlay PUBLIC zcsr_platform)
if(WIN32)
  target_link_libraries(zcsr_debug_overlay PUBLIC user32 gdi32)
endif()

add_executable(test_overlay tests/test_overlay.c)
target_link_libraries(test_overlay PRIVATE zcsr_debug_overlay)
add_test(NAME overlay COMMAND test_overlay)

add_executable(test_overlay_integration tests/test_integration.c)
target_link_libraries(test_overlay_integration PRIVATE
    zcsr_debug_overlay
    zcsr_core
    zcsr_hsm
    zcsr_signal)
add_test(NAME overlay_integration COMMAND test_overlay_integration)
