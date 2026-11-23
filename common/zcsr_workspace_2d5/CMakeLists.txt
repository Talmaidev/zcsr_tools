cmake_minimum_required(VERSION 3.16)
project(zcsr_workspace_2d5 LANGUAGES C)

add_library(zcsr_workspace_2d5 STATIC
    src/workspace_2d5.c
    src/light_key_contract.c)

target_include_directories(zcsr_workspace_2d5 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(zcsr_workspace_2d5 PRIVATE
    ${ZCSR_ENGINE_INCLUDE}
    ${ZCSR_RUNTIME_INCLUDE}
    ${ZCSR_UIM_PAGE_MANAGER_INCLUDE})

if(ZCSR_ENGINE_INCLUDE)
  get_filename_component(ZCSR_ENGINE_ROOT "${ZCSR_ENGINE_INCLUDE}" DIRECTORY)
  target_include_directories(zcsr_workspace_2d5 PRIVATE
      ${ZCSR_ENGINE_ROOT}/third_party/glad/include)
endif()

if(ZCSR_ENGINE_INCLUDE)
  add_executable(light_key_contract_test tests/test_light_key_contract.c
      src/light_key_contract.c)
  target_include_directories(light_key_contract_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${ZCSR_ENGINE_INCLUDE})
  add_test(NAME light_key_contract COMMAND light_key_contract_test)
endif()
