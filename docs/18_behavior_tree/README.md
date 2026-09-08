# 行为树：把“下一步做什么”写成可组合的任务清单

位置：[主线](../00_course_map.md)顺序 5。前一站：[Lifecycle](../17_lifecycle/README.md)；下一站：[配送机器人 V1](../task_orchestration_capstone.md)。

状态：入门正文、纯 C++ 概念演示可用；BehaviorTree.CPP、XML、ROS action 与 Nav2 集成待实现。

## 第一遍读法与先记三件事

先手推三轮执行，再运行小程序；第一次不用安装 Nav2 或树编辑器。

1. 行为树（Behavior Tree，BT）像能反复检查的任务清单，用树形结构组合条件和动作。
2. 每次 tick 是“推进/检查一轮”，返回成功 `SUCCESS`、失败 `FAILURE` 或仍在进行 `RUNNING`。
3. `RUNNING` 不等于开了一个线程；长任务需要及时返回，并支持被上层中断。

类比边界：清单自己不会动，需要外部循环或调度器 tick；树节点也不是 ROS node。真正导航由动作服务端完成，树负责决定何时请求、等待和停止。

## 从一句需求搭出树

需求：**电量足够才配送；优先走主路线，主路线不可用时尝试绕行；途中电量不足就中断本次配送。** 本入门树不会自动返航，上层收到失败后再决定下一步。

```text
ReactiveSequence（每轮重新检查前置条件）
├── BatteryOK?（电量至少 30%）
└── Fallback（按顺序找可行路线；RUNNING 时继续当前路线）
    ├── MainRoute（模拟走主路线）
    └── Detour（模拟绕行）
```

| 节点 | 像什么 | 本课采用的语义 |
|---|---|---|
| Condition | 检查票据 | 根据当前数据返回成功/失败，不等待长任务 |
| Action | 办一件事 | 开始后可跨多轮运行，完成或失败时返回终态 |
| Sequence | 按顺序完成几步 | 子节点失败则整段失败；全部成功才成功；RUNNING 时暂停推进后续步骤 |
| Fallback / Selector | 依次试候选方案 | 子节点失败才试下一个；一个成功则成功；RUNNING 时还不能宣布失败 |
| ReactiveSequence | 每次先重新查通行条件 | 每轮从头检查；前置条件失败时 halt 后方仍运行的分支 |
| Decorator | 给一个步骤加规则 | 例如有限次数重试或超时；规则不能替代动作自身取消处理 |

不同框架、不同控制节点对“下一轮从哪里继续”的规定不同。普通 Sequence 在子动作 RUNNING 时通常继续该动作，不能据此声称它会每轮重查之前成功的电量条件。本课选 ReactiveSequence，就是为了显式表达重查。概念与对照参考 [BehaviorTree.CPP 基础](https://www.behaviortree.dev/docs/learn-the-basics/BT_basics/)及 [3.8 响应式与异步行为教程](https://www.behaviortree.dev/docs/3.8/tutorial-basics/tutorial_04_sequence/)。

## 动手：三条路径，逐 tick 看

从仓库根目录，使用 CMake 与 C++17 编译器，无需 ROS：

```bash
cmake -S labs/task_orchestration -B build/task_orchestration
cmake --build build/task_orchestration --parallel 2
build/task_orchestration/bt_delivery normal
build/task_orchestration/bt_delivery low_battery
build/task_orchestration/bt_delivery blocked
```

[演示代码](../../labs/task_orchestration/bt_delivery.cpp)显式实现本课所需的控制语义。它不是通用 BT 框架，不承诺兼容某个框架版本的完整状态、调度、异常或 XML 语义。

先猜：主路线仍在 RUNNING 时，是否应该同时启动绕行？第二轮电量降为 20%，之前走到一半的主路线是否还可以继续？

| 情景 | 第 1 轮 | 第 2 轮 | 后续 |
|---|---|---|---|
| normal | 电量成功；主路线 START → RUNNING | 重查电量；主路线 RUNNING | 第 3 轮 SUCCESS，根结束 |
| low_battery | 主路线 RUNNING | 电量 20；主路线 HALT；根 FAILURE | 模拟管理者恢复电量并明确重试；第 3～5 轮重新走完 |
| blocked | 主路线 FAILURE；绕行 START → RUNNING | 从正在运行的绕行继续 | 第 3 轮 SUCCESS |

预期每个情景末尾为 `BT checks passed: ...`。轮次和状态是固定教学输入，不是实时性能指标。low_battery 的恢复由程序显式触发，不表示树会自行充电、自行重试。

**可证伪判断**：normal 中 `MainRoute START` 应只有一次；low_battery 第二轮必须出现 HALT，且不能转而尝试绕行；blocked 中主路线失败不应让根立即失败，因为还有候选路线。

**修改练习**：让绕行也可能失败，预测根的结果；把电量阈值提高，观察任务是否根本没有启动。再设计“最多重试两次”的装饰规则，明确耗尽预算后的去向。不要用无限 retry 掩盖持续故障。

**异常与清理**：找不到可执行文件时先检查构建目录。程序应在有限轮次内退出；输入不存在的情景会返回错误并打印用法。没有后台线程、网络连接或设备资源需清理。

## 第二遍：从模拟动作接到 ROS action

在工程版本中，`Move` 不再用计数器表示进度，而是持有 action 客户端与当前目标信息：

| 时刻 | 树侧应做什么 | 不能混淆的事实 |
|---|---|---|
| 第一次进入动作 | 异步发送一次 goal，记下本次任务标识 | 发送请求不等于服务端接受 |
| 后续 tick | 检查接受/反馈/结果，未结束返回 RUNNING | 每次 tick 都发 goal 会重复启动任务 |
| 收到结果 | 按 succeeded/aborted/canceled 映射业务结果 | ROS 的 canceled 不是配送成功 |
| 上层 halt | 请求取消并进入应用的取消确认流程 | 本地树停止 tick 不等于远端停止运动 |
| 取消超时或服务端失联 | 按应用策略进入失败/停止确认状态 | 不能立刻假定旧目标不存在并发新目标 |

取消可能发生在 goal 还没被接受时；应记录取消意图，在拿到 goal handle 后继续取消，屏蔽旧任务的迟到回调。不要在 tick/halt 中无限阻塞等待响应，否则处理结果所需的 Executor 可能也被堵住。

代码里的 HALT 是同步停止一个内存计数器，不能拿它证明真实 Action 的取消时序。后续实现必须记录 request、accepted、result 和任务 ID，并做集成验证。

## 第三遍：Blackboard、框架与 Nav2（后续学习）

黑板（Blackboard）是在树节点之间传递上下文的共享数据区，例如目标、电量及其时间戳。输入/输出端口（Ports）让数据依赖可见。黑板既不是 ROS 参数服务器，也不自动保证线程安全；从 ROS 回调读取数据时仍要设计快照或同步机制。

推荐扩展顺序：先把本课树迁移到固定版本的 BehaviorTree.CPP → 注册自定义条件与动作 → 学端口和黑板 → 写非阻塞长动作与 halt → 接 ROS action → 观察状态日志 → 再读 Nav2 的导航与恢复树。

集成前记录目标 Humble 环境中实际安装的 `behaviortree_cpp_v3`/其他 BT 库及版本。3.x 与 4.x 的包名、API 和 XML 约定不能直接混用，也不要把新版特性当成 Humble 已具备。本课不提供未经编译验证的框架 XML。具体语义以所选版本文档和源码为准。

## 与状态机、Lifecycle 怎样合作

配送 FSM 决定任务处于 `Idle/Delivering/Canceling/Fault`；节点 Lifecycle 说明导航服务是否配置且激活；BT 在允许配送时组合导航、检查和候选策略。通常只在需要更换任务策略时修改 BT，不为每次订单完成清理整个导航节点。

简单、固定的几种业务模式，用 FSM 已经很清楚；有重复子任务、多条尝试路径、条件重评估时，BT 更容易组合。先根据问题选工具，不要求每个节点同时嵌套三套框架。

## 过关

- [ ] 能手推三种情景每轮 tick 的返回值与执行路径。
- [ ] 能解释 Fallback 为什么不能把 RUNNING 当失败。
- [ ] 能解释响应式条件检查与普通顺序执行的差别。
- [ ] 能说明树 halt、取消请求发出、远端取消完成的不同时间点。

下一站：[配送机器人 V1 设计与验收](../task_orchestration_capstone.md)。
