# zcsr_tools

`zcsr_tools` is a **developer tool layer**, not a product entry and not a
window host. It contains reusable tool code and one domain pack. The runtime
host owns the OS window and main loop; the editor workspace owns authoring UI.

This README uses these words with exactly one meaning:

- **tool layer**: reusable non-product code that a game or editor may link.
- **domain pack**: code for one game family, for example tower defense.
- **2.5D workspace reference**: a fixed demonstration of floor, wall and box
  geometry. It is not a general scene authoring system.
- **scene authoring**: the future document, command and serialization layer for
  a 2.5D scene. It is not present in this repository yet.

## common/

Reusable tool code. A game links only the targets it uses; a tool target does
not force another tool target.

- `zcsr_game_two_dimensional`: 2D point, grid, board coordinate and caller-owned
  occupancy view.
- `zcsr_pathfinding`: graph A*/Dijkstra path solving with caller-owned workspace
  and output buffers.
- `zcsr_flow_field`: uniform-cost grid integration/flow field with caller-owned
  cost, distance, queue and direction storage.
- `zcsr_spatial_grid`: uniform-grid broadphase for caller-owned AABBs, with
  point and AABB candidate queries; it does not decide collisions or targets.
- `zcsr_bench`: CPU/GPU/custom sample series and p50/p95/p99 report.
- `zcsr_profiler`: fixed-window FPS, counters and p95 HUD.
- `zcsr_debug_overlay`: developer-only standalone debug view. It is not loaded
  into the product runtime and never becomes a second product window.
- `zcsr_debug_draw`: debug primitive command buffer.
- `zcsr_workspace_2d5`: fixed 2.5D light workspace reference. It is not scene
  authoring and it is not linked by the editor today. It is built and tested in
  this repository but is not exported by the `zcsr_tools` install package because
  its demo shell consumes the runtime host.

Constraint: no `tower_defense`, `pvz`, `plant`, `zombie`, `sun` or other domain
instance names in `common/`; no dependency on `tower_defense/`.

## tower_defense/

塔防大类必备功能，只放所有塔防游戏会重复实现的调度原语；不包装公共 2D，也不依赖公共 2D 才能使用。

- `zcsr_tower_defense`：固定容量波次生成调度、lane 限制。

约束：不出现 `pvz`、`plant`、`zombie`、`sun` 等实例名词；不读取 PVZ 数据、美术或数值。卡片经济、阳光消费、冷却、特殊技能、flag/boss 事件属于 game 内独立管理，不进工具层。

## 消费方式

- 新塔防游戏：按需同时链接 `zcsr_tower_defense` 和 `zcsr_game_two_dimensional`；两个模块独立，不强制一条链，也不互相包装。
- 新非塔防游戏：只链接需要的 `common` 工具，不引入塔防层。
- PVZ：`pvz_input_map` 和 `pvz_sim` 直接调用 `zcsr_game_two_dimensional` 的网格坐标、占用、格子中心；lane 限制调用 `zcsr_tower_defense_lane_clamp`，波次调用 `zcsr_tower_defense_wave_schedule`。PVZ 不再保留自己的格子除法、占用检查、lane clamp 或 `next_wave < wave_count` 循环。

`tests/check_layer_boundary.cmake` 会在 CTest 中检查目录边界，防止后续把实例逻辑塞回通用层；当前 tools CTest 11/11。`zcsr_bench` 只负责采样和 p50/p95/p99 报表，不是发布性能结论；跨引擎对比必须使用同一热机、同分辨率、同资产的单一控制变量表。
