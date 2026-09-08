# Stage 03 — DDS、RTPS 与 Discovery（规划）

重要度：⭐⭐⭐　预计：12～16 小时　依赖：Stage 02

> **白话预告**：两个程序没有中央通讯录时，怎样互相找到并确认“你发送的正是我想接收的数据”？本阶段会用运行日志和抓包回答。当前只有规划，缩写不必现在记；可查[术语表](../00_glossary.md)。

## 核心问题

没有 ROS Master，两个刚启动的进程如何获知对方、交换 endpoint 元数据并决定是否匹配？

## 学习目标

- 映射 Node/Publisher/Subscription 到 DDS 实体：Publisher/Subscription 通常落实为 DataWriter/DataReader，同时明确 ROS Node 与 DomainParticipant 并非规范要求的一一对应，participant 可由 RMW/context 复用；
- 解释 GUID prefix、EntityId、locator、builtin endpoint；
- 分开描述 SPDP participant discovery 和 SEDP endpoint discovery；
- 用 topic/type/Domain/QoS/partition 的条件解释 endpoint matching；
- 识别 RTPS DATA、DATA_FRAG、HEARTBEAT、ACKNACK。

## 实验列表

| ID | 实验 | 操作 |
|---|---|---|
| A03-01 | talker/listener discovery 抓包 | tcpdump + Wireshark `rtps` |
| A03-02 | Domain 隔离 | `ROS_DOMAIN_ID=10` vs `20` |
| A03-03 | 晚加入端点 | 绘制 SPDP→SEDP→DATA 时间线 |
| A03-04 | multicast 故障 | firewall/netns 阻断并观察 |
| A03-05 | QoS endpoint 不兼容 | 图可见但不匹配 |
| A03-06 | vendor 对照 | Fast DDS 与 Cyclone DDS 抓包 |

## 实验猜想

在只阻断 discovery multicast、保留已知 unicast 通道时，新节点和已匹配节点会分别怎样？先写出猜想，再按 discovery plane/data plane 分析。

## 【求职价值】

多机“发现慢、看得见但收不到”的根因常在 SPDP、SEDP、QoS matching 和 user-data locator 的不同阶段。能读 discovery 抓包是 DDS 岗位的硬证据。

## Humble 边界

抓包字段与默认 locator 受 DDS vendor 和版本影响；正文将固定 apt 包版本并将新版本 discovery server/配置变化单列。

## 完成定义

- [ ] 能从 pcap 指出一次 participant 和 endpoint discovery。
- [ ] 能解释 ROS Master 与 distributed discovery 的故障域差别。
- [ ] 能解释相同 topic name 为什么仍可能不通信。
