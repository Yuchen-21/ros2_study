# 状态机与行为树：无 ROS 依赖的概念实验

配套教程：[状态机](../../docs/16_state_machine/README.md)、[行为树](../../docs/18_behavior_tree/README.md)。学习顺序见[课程路线](../../docs/00_course_map.md)。

这里只模拟订单事件和逐 tick 决策，不驱动机器人，不使用 BehaviorTree.CPP，不模拟 DDS 或 Lifecycle 管理服务。C++ 示例帮助理解规则；ROS Action 的异步取消确认必须在后续集成中单独实现和测试。

从仓库根目录执行，需 CMake ≥ 3.16 和 C++17 编译器：

```bash
cmake -S labs/task_orchestration -B build/task_orchestration
cmake --build build/task_orchestration --parallel 2
build/task_orchestration/fsm_delivery
build/task_orchestration/bt_delivery normal
build/task_orchestration/bt_delivery low_battery
build/task_orchestration/bt_delivery blocked
cmake -E chdir build/task_orchestration ctest --output-on-failure
```

FSM 输出每次事件导致的转移或拒绝；BT 输出每轮条件、开始/运行/停止和根节点结果。先按教程写猜想，再与输出对照。每个程序内有逐步业务检查，错误返回非零；测试成功应显示 4/4 passed。

使用 `cmake -E chdir` 兼容旧版 CTest；部分旧版不支持 `--test-dir`，会在错误目录输出 `No tests were found`，这不算验证通过。

| 测试 | 验证内容 |
|---|---|
| fsm_transitions | 低电量门槛、取消/返航、成功、故障复位、非法/重复事件不乱跳 |
| bt_success | 三轮完成，一次尝试只开始一次动作，备用路线未启动 |
| bt_low_battery_interrupt | 第二轮前置条件失败会 halt；恢复后的新尝试从头开始 |
| bt_fallback | 主路线失败，备用路线跨 tick 持续运行且只启动一次 |

没有 ROS 包注册，不使用 colcon 构建。命令找不到时检查当前路径和构建输出；所有程序应自行退出，没有后台服务、socket 或需要人工回收的共享资源。构建产物位于 `build/task_orchestration`，重复执行构建即可复用。
