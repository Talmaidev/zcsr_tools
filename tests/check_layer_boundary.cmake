# zcsr_tools 分层边界检查。
# common 只能放通用能力；tower_defense 只能放塔防通用能力，不能把 PVZ 名词泄漏进工具层。

set(common_directory "${CMAKE_CURRENT_LIST_DIR}/../common")
set(tower_defense_directory "${CMAKE_CURRENT_LIST_DIR}/../tower_defense")

file(GLOB_RECURSE common_sources
    "${common_directory}/*.c"
    "${common_directory}/*.h")
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

message(STATUS "zcsr_tools layer boundary: PASS")
