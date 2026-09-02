# Stage 11 — ROS2 DDS 与其他中间件的设计比较（规划）

重要度：⭐⭐⭐　预计：10～16 小时　依赖：Stage 02～10

## 核心原则

不做“功能打勾表”。比较时先固定场景、消息、拓扑、可靠性目标、硬件和故障模型，再讨论 DDS、CyberRT、LCM、MQTT、Zenoh、ZeroMQ 或裸共享内存。

## 学习目标

- 区分 data-centric pub/sub、brokered messaging、router-based fabric、communication library；
- 比较 discovery、transport、serialization、QoS、routing、SHM、实时性、运维和生态；
- 识别同机 data plane、跨机 control/data plane、云边 bridge 的不同约束；
- 设计 bridge 的背压、时间戳、schema、重连和观测。

## 实验与设计题

- 对统一 SensorData adapter 做功能与故障语义实验；
- 比较无订阅者、慢订阅者、网络分区、late joiner；
- 分析 benchmark 公平性；
- 为 LiDAR/Camera/INS/Planning/Control/CAN/Foxglove 写 ADR；
- 设计 DDS + SHM + bridge + WebSocket 混合架构；
- 分析 MCAP 记录对实时路径的隔离。

## 目标架构讨论

```text
LiDAR/Camera ─ SHM or DDS data plane ─ Perception
INS ──────────────────────────────── DDS ─ Localization
Planning ─ DDS ─ Control ─ bounded local queue ─ CAN ─ Vehicle
                         │
                         └─ monitoring/recording bridge ─ WebSocket ─ Foxglove
```

可视化和记录不应无边界地反压关键控制链；bridge 必须有降采样/队列/丢弃策略和独立资源预算。

## 【求职价值】

架构岗位不仅问“哪个更快”，还问 failure semantics、组织成本、生态兼容和运维。能给出约束下的取舍，比站队某个中间件更专业。

## 完成定义

- [ ] 能说明各系统原生解决的问题和不负责的问题。
- [ ] 能给出混合架构中每条 bridge 的语义损失。
- [ ] 能写包含 alternatives/consequences 的 ADR。
