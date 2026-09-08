# Stage 09 — 网络、多机、Docker 与 namespace（规划）

重要度：⭐⭐⭐　预计：14～20 小时　依赖：Stage 03、05、08

> **白话预告**：消息跨过机器、容器和网卡，就像经过更多路口。本阶段会在两端留下证据，定位它究竟在哪个路口停下或丢失。当前只有规划；术语见[术语表](../00_glossary.md)。

## 核心问题

为什么两台机器上 `ros2 topic list` 有时互相可见，`echo` 却无数据；为什么小 topic 正常而 PointCloud 丢失？

## 学习目标

- 画出 process → socket → route → qdisc → veth/bridge/NIC；
- 分离 multicast discovery 和 user-data path；
- 理解 MTU、fragmentation、socket buffer、Wi-Fi 与 Docker 网络；
- 使用 netns/tc/firewall 可控地制造故障；
- 配置 DDS interface allowlist 与静态/服务发现方案。

## 实验拓扑

```text
[sensor_ns: 10.20.0.2] ─ veth ─ [Linux bridge/router] ─ veth ─ [robot_ns: 10.20.0.3]
        tcpdump A                    tc/firewall                   tcpdump B
```

## 实验列表

- namespace/container 基线发现；
- 阻断 multicast；
- 单向 firewall；
- loss/delay/jitter/rate；
- MTU 1500/1200 不一致；
- UDP receive buffer 压力；
- Docker bridge、host、macvlan 语义对比；
- 多 NIC 错误选择；
- pcap 时间线与 DDS log 对照。

## 【求职价值】

机器人多机系统横跨 Linux 网络、容器和 DDS。能确认 packet 在哪一个 interface/namespace 消失，比反复改 DDS XML 更重要。

## 安全边界

`tc`、netns 和 firewall 实验只操作课程创建的 namespace/veth，不修改默认网卡；脚本提供幂等清理和 dry-run。

## 完成定义

- [ ] 能解释“发现成功、数据失败”的至少五类原因。
- [ ] 能在两端同时抓包并定位丢失边界。
- [ ] 能预测大样本与 MTU/fragment loss 的放大效应。
