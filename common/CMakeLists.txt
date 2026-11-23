cmake_minimum_required(VERSION 3.16)

# 通用必备层：所有 2D/非 2D 游戏都能直接复用，不含任何塔防语义。
add_subdirectory(zcsr_game_two_dimensional)
add_subdirectory(zcsr_pathfinding)
add_subdirectory(zcsr_flow_field)
add_subdirectory(zcsr_spatial_grid)
add_subdirectory(zcsr_debug_overlay)
add_subdirectory(zcsr_workspace_2d5)
add_subdirectory(zcsr_profiler)
add_subdirectory(zcsr_debug_draw)
add_subdirectory(zcsr_bench)
