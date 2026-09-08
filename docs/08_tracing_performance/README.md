# Stage 08 — ROS2 Tracing 与 Communication Profiler（规划）

重要度：⭐⭐⭐　预计：16～24 小时　依赖：Stage 04～07

> **白话预告**：一条消息晚了 12 毫秒，怎样像给接力赛每一棒计时那样，找出时间究竟花在发送、搬运、排队还是处理上？当前只有规划；性能词汇可查[术语表](../00_glossary.md)。

## 核心问题

一次 callback 晚了 12 ms：是 publisher 晚、serialize 慢、DDS 排队、网络晚、Executor 没线程，还是 callback 自身执行久？

## 学习目标

- 定义 latency、jitter、throughput、callback duration、queue delay、scheduling delay；
- 理解 LTTng session/context/clock 与 tracetools event；
- 将 message sequence/timestamp 和 `rclcpp_callback_register`、`rmw_publish`、callback start/end 关联；
- 使用 perf 与 `/proc` 补足 CPU、fault、scheduler 证据；
- 认识 probe overhead 和观察者效应。

## 实验列表

- talker/listener 全链路 trace；
- callback object 与 symbol/topic 关联；
- publish-to-callback、ready-to-start、callback duration 分布；
- perf stat/record + flamegraph；
- CPU affinity/负载注入；
- C++ monitor node + Python trace analyzer；
- 输出 JSON/CSV 与 P50/P95/P99/max；
- 同步时钟不可信时的 RTT/同机测量替代。

## Profiler 最小指标

`Topic Hz | message size | bandwidth | sequence gap | inter-arrival | message age | callback duration | CPU | RSS`

每个指标都必须注明测量位置与时钟域。

## 【求职价值】

性能分析不是贴一张 `ros2 topic hz` 截图。工程核心是建立时间线、划分预算、控制变量并用多个观测面交叉验证。

## 完成定义

- [ ] 能从 trace 分离 callback duration 和 scheduling delay。
- [ ] 能解释 trace 中 `rmw_publish` 不是必然的物理发包时刻。
- [ ] profiler 在高负载下有有界资源使用和明确采样开销。
