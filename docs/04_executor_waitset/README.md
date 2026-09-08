# Stage 04 — Executor、WaitSet 与 CallbackGroup（规划）

重要度：⭐⭐⭐　预计：12～16 小时　依赖：Stage 02

> **白话预告**：消息已经放进收件箱，不代表处理函数马上运行。本阶段会回答谁负责等待、谁唤醒工作线程，以及一个很慢的任务为什么会拖住其他任务。当前只有规划；术语见[术语表](../00_glossary.md)。

## 核心问题

DDS 已经收到样本后，究竟是谁唤醒谁、谁选择 callback、哪个线程执行；慢 callback 为什么会拖累另一个 topic？

## 学习目标

- 连接 Entity → rmw/rcl readiness → WaitSet → Executor → CallbackGroup → callback；
- 精确回答 DDS receive thread 通常不直接调用普通 rclcpp callback；
- 比较 `spin`、`spin_once`、`spin_some`；
- 比较 SingleThreaded、MultiThreaded 与 Humble StaticSingleThreadedExecutor；
- 分析 mutually-exclusive/reentrant、线程数、阻塞和死锁。

## 实验列表

| ID | 实验 | 变量 |
|---|---|---|
| A04-01 | 两订阅 + timer | timestamp、TID、callback name |
| A04-02 | executor 对照 | single/multi/static |
| A04-03 | 500 ms 慢 callback | callback age 与其他任务间隔 |
| A04-04 | callback group 矩阵 | mutually-exclusive/reentrant |
| A04-05 | 同步 service/future 死锁 | group 与 executor threads |
| A04-06 | WaitSet 手动调度 | readiness 与选择策略 |

## 实验猜想

两个 callback 放入同一个 MutuallyExclusive group，即使 MultiThreadedExecutor 有四个线程，是否能并行？如果不能，线程都在做什么？

## 🔎 Source Dive

沿 `rclcpp::executors::SingleThreadedExecutor::spin`、`Executor::get_next_executable`、`wait_for_work`、`rcl_wait` 阅读；每次只回答“如何等待、如何选任务、如何占用 callback group”之一。

## 【求职价值】

控制周期抖动、callback starvation、priority inversion 和 service deadlock 都要求理解调度模型，而不只是“换 MultiThreadedExecutor”。

## 完成定义

- [ ] 能画 readiness 到 callback 的线程时序。
- [ ] 能预测六种 executor/group 组合下的并发结果。
- [ ] 能用 trace 区分 callback duration 与 scheduling delay。
