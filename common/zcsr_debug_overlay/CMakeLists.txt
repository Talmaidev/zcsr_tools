cmake_minimum_required(VERSION 3.16)

add_library(zcsr_debug_overlay STATIC src/overlay.c)
target_include_directories(zcsr_debug_overlay PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(ZCSR_ENGINE_INCLUDE)
  target_include_directories(zcsr_debug_overlay PUBLIC ${ZCSR_ENGINE_INCLUDE})
endif()
if(WIN32)
  target_link_libraries(zcsr_debug_overlay PUBLIC user32 gdi32)
endif()

add_executable(test_overlay tests/test_overlay.c)
target_link_libraries(test_overlay PRIVATE zcsr_debug_overlay)
if(WIN32)
  target_sources(test_overlay PRIVATE
      ${ZCSR_ENGINE_DIR}/modules/platform/platform_common.c
      ${ZCSR_ENGINE_DIR}/modules/platform/platform_win32.c)
  target_include_directories(test_overlay PRIVATE
      ${ZCSR_ENGINE_DIR}/modules/platform)
endif()
add_test(NAME overlay COMMAND test_overlay)
