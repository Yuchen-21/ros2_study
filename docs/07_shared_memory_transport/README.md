# Stage 07 — ROS2/DDS Shared Memory Transport（规划）

重要度：⭐⭐　预计：10～14 小时　依赖：Stage 05、06

> **白话预告**：同一台机器上的两个 ROS 2 进程怎样共用“仓库”，少搬几次相机图像或点云？本阶段还会验证数据是否真的走了共享内存。当前只有规划；术语见[术语表](../00_glossary.md)。

## 核心问题

两个 ROS2 进程发送 5 MB 样本时，Fast DDS shared-memory transport 如何定位 segment、通知 reader 并管理 buffer？它和 intra-process/loaned message是什么关系？

## 学习目标

- 理解 transport descriptor、locator、segment、port 与 notification；
- 明确“走 SHM”和“端到端零复制”不是同一命题；
- 掌握 UDP-only/SHM-enabled 的可证伪配置；
- 观察 `/dev/shm`、maps、CPU、RSS、吞吐和 tail latency；
- 处理 crash cleanup、权限和容器 IPC namespace。

## 实验列表

| ID | 实验 | 对照 |
|---|---|---|
| A07-01 | 5 MB 两进程 baseline | 默认 transport |
| A07-02 | 强制 UDP-only | socket/fragment/CPU |
| A07-03 | SHM-only 或优先 SHM | segment/maps/CPU |
| A07-04 | 容器 IPC namespace | host/private IPC |
| A07-05 | writer crash | stale segment/恢复 |
| A07-06 | Image/PointCloud 容量预算 | memory bandwidth |

## 【求职价值】

相机和点云会先撞上 memory bandwidth、serialization 与 cache，而不是只看 topic Hz。能证明数据实际走哪条 transport 是调优前提。

## Humble 行为

正文会锁定 Ubuntu 22.04 apt 中 Fast DDS 与 `rmw_fastrtps_cpp` 版本并提供 XML。上游 Fast DDS 新版本的 Data-sharing/SHM 配置、默认值和工具会单列，不能直接复制到 Humble 假设可用。

## 完成定义

- [ ] 能以 segment/socket 证据证明传输路径。
- [ ] 能解释容器 `--ipc` 对 SHM 的影响。
- [ ] 能同时报告 median 和 P99，而不是只报峰值带宽。
