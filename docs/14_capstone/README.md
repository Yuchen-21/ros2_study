# Stage 14 — mini_robot_middleware_lab（规划）

重要度：⭐⭐⭐　预计：30～50 小时　依赖：Stage 01～12

## 系统节点与负载

| Node | Rate | 数据特征 | 初始通信策略 |
|---|---:|---|---|
| lidar_node | 10 Hz | 大 PointCloud-like | sensor QoS，SHM 对照 |
| camera_node | 30 Hz | 大 Image-like | best effort，bounded queue |
| localization_node | 100 Hz | 小、低延迟 | reliable/volatile，独立 group |
| planning_node | 20 Hz | 中等结果 | reliable，freshness 监控 |
| control_node | 100 Hz | 小、deadline 敏感 | 有界队列，CAN adapter 隔离 |
| monitor_node | sampling | 元数据与系统指标 | 不反压关键链 |
| foxglove_bridge | 可配置 | WebSocket/可视化 | 降采样与独立资源 |

## 必须实现

- 自定义接口、QoS profile 和配置说明；
- MultiThreadedExecutor + CallbackGroup 设计及其理由；
- message sequence、source timestamp、callback duration instrumentation；
- LTTng/tracetools trace；
- Fast DDS SHM/UDP 对照；
- network namespace/tc 故障注入；
- monitor：Hz、bandwidth、latency/age、inter-arrival、callback duration、CPU、RSS；
- MCAP/Foxglove 可观测路径；
- 自动化实验报告和复现命令。

## 故障场景

1. 5% packet loss；
2. localization callback 随机 sleep；
3. QoS mismatch；
4. background CPU overload；
5. 40 ms ± 15 ms network delay；
6. 大消息使 socket/history resource 紧张；
7. 单一 callback group 导致 control starvation；
8. monitor/bridge 消费过慢。

学习者只收到症状和 SLO，不提前知道注入参数；定位后必须给出证据、修复和同条件回归。

## 目标 SLO（需按硬件校准）

- control freshness/deadline；
- localization P99 message age；
- sensor drop policy；
- bridge 最大 CPU/RSS；
- 故障恢复时间；
- monitor 自身 overhead。

数字不在规划阶段写死。第一轮 baseline 后记录硬件与负载，再形成有依据的阈值。

## 【求职价值】

这个项目的展示重点不是“有七个节点”，而是需求 → 架构决策 → 实现 → trace/pcap → 故障注入 → 定量优化的闭环。

## 完成定义

- [ ] 全系统一条命令启动、停止和收集报告。
- [ ] 每个 SLO 有测量口径与 failure policy。
- [ ] 八个故障可重复，修复有回归。
- [ ] README 能在 10 分钟内支持现场演示。
