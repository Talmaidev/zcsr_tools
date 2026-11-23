# zcsr_tools 分层边界检查。
# common 只能放通用能力；tower_defense 只能放塔防通用能力，不能把 PVZ 名词泄漏进工具层。

set(common_directory "${CMAKE_CURRENT_LIST_DIR}/../common")
set(tower_defense_directory "${CMAKE_CURRENT_LIST_DIR}/../tower_defense")

file(GLOB_RECURSE common_sources
    "${common_directory}/*.c"
    "${common_directory}/*.h"
    "${common_directory}/*.py")
foreach(source_file IN LISTS common_sources)
    file(READ "${source_file}" source_text)
    string(TOLOWER "${source_text}" source_text_lower)
    foreach(forbidden_term "tower_defense" "pvz" "plant" "zombie" "sun")
        string(FIND "${source_text_lower}" "${forbidden_term}" match_index)
        if(NOT match_index EQUAL -1)
            message(FATAL_ERROR
                "common layer leaks genre term '${forbidden_term}': ${source_file}")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE tower_defense_sources
    "${tower_defense_directory}/*.c"
    "${tower_defense_directory}/*.h")
foreach(source_file IN LISTS tower_defense_sources)
    file(READ "${source_file}" source_text)
    string(TOLOWER "${source_text}" source_text_lower)
    foreach(forbidden_term "pvz" "plant" "zombie" "sun")
        string(FIND "${source_text_lower}" "${forbidden_term}" match_index)
        if(NOT match_index EQUAL -1)
            message(FATAL_ERROR
                "tower_defense layer leaks PVZ term '${forbidden_term}': ${source_file}")
        endif()
    endforeach()
endforeach()

# Cross-repo boundary: tools may consume engine CMake targets, but must not
# compile engine module .c files into tool executables. That bypasses the
# public zcsr API and silently drags Win32-only implementation into Linux.
file(GLOB_RECURSE tool_cmake_files
    "${CMAKE_CURRENT_LIST_DIR}/../common/*/CMakeLists.txt"
    "${CMAKE_CURRENT_LIST_DIR}/../tower_defense/*/CMakeLists.txt")
foreach(cmake_file IN LISTS tool_cmake_files)
    file(READ "${cmake_file}" cmake_text)
    string(FIND "${cmake_text}" "ZCSR_ENGINE_DIR}/modules" match_index)
    if(NOT match_index EQUAL -1)
        message(FATAL_ERROR
            "tools compile engine internals instead of linking its target: ${cmake_file}")
    endif()
endforeach()

message(STATUS "zcsr_tools layer boundary: PASS")
