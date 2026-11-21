# zcsr_tools 分层

`zcsr_tools` 只允许两个独立功能层，新游戏按需组合，不允许复制整套工具，也不强迫所有公共能力经过某一层转发。

## common/

通用必备功能，任何 2D 或非 2D 游戏都可以直接复用。

- `zcsr_game_two_dimensional`：通用 2D 点、格子、棋盘坐标、调用方存储的占用视图。
- `zcsr_bench`：CPU/GPU/自定义序列采样与 p50/p95/p99 报表。
- `zcsr_profiler`：固定窗口 FPS、计数器、p95 HUD。
- `zcsr_debug_overlay`：独立原生调试窗口。
- `zcsr_debug_draw`：调试图元命令缓冲。
- `zcsr_workspace_2d5`：2.5D 灯光工作区参考实现。

约束：不出现 `tower_defense`、`pvz`、`plant`、`zombie`、`sun` 等大类或实例语义；不依赖 `tower_defense`。

## tower_defense/

塔防大类必备功能，只放所有塔防游戏会重复实现的调度原语；不包装公共 2D，也不依赖公共 2D 才能使用。

- `zcsr_tower_defense`：固定容量波次生成调度、lane 限制。

约束：不出现 `pvz`、`plant`、`zombie`、`sun` 等实例名词；不读取 PVZ 数据、美术或数值。卡片经济、阳光消费、冷却、特殊技能、flag/boss 事件属于 game 内独立管理，不进工具层。

## 消费方式

- 新塔防游戏：按需同时链接 `zcsr_tower_defense` 和 `zcsr_game_two_dimensional`；两个模块独立，不强制一条链，也不互相包装。
- 新非塔防游戏：只链接需要的 `common` 工具，不引入塔防层。
- PVZ：`pvz_input_map` 和 `pvz_sim` 直接调用 `zcsr_game_two_dimensional` 的网格坐标、占用、格子中心；lane 限制调用 `zcsr_tower_defense_lane_clamp`，波次调用 `zcsr_tower_defense_wave_schedule`。PVZ 不再保留自己的格子除法、占用检查、lane clamp 或 `next_wave < wave_count` 循环。

`tests/check_layer_boundary.cmake` 会在 CTest 中检查目录边界，防止后续把实例逻辑塞回通用层；当前 tools CTest 9/9。`zcsr_bench` 只负责采样和 p50/p95/p99 报表，不是发布性能结论；跨引擎对比必须使用同一热机、同分辨率、同资产的单一控制变量表。
