# Stage 06 — Serialization、Intra-process 与 Zero Copy（规划）

重要度：⭐⭐⭐　预计：14～20 小时　依赖：Stage 02、04、05

## 核心问题

1 MB ROS message 从用户对象到 DDS sample 发生了哪些 allocation/copy/serialization？“零拷贝”究竟消除了哪一段？

## 学习目标

- 追踪 `.msg/.idl` → rosidl generator → typesupport → CDR；
- 理解 alignment、endianness、bounded/unbounded type；
- 区分 intra-process、inter-process、SHM transport、loaned message、zero copy；
- 识别 ownership、lifetime 和 fallback；
- 用数据量变化建立 CPU/latency/throughput 模型。

## 实验列表

- 定义含 Header 与 `float64[]` 的 PointCloud-like message；
- 发送 1 KB、100 KB、1 MB、10 MB；
- 记录 CPU、RSS、minor faults、latency、throughput；
- typed vs pre-serialized；
- composition + intra-process on/off；
- `borrow_loaned_message` capability probe；
- bounded/fixed-size 与 dynamic sequence 对照；
- allocator 统计和 perf memory hotspot。

## 实验猜想

同一进程启用 intra-process 后，`unique_ptr` 消息是否对多个订阅者仍能保持“同一对象、零复制”？先考虑扇出和所有权，再实验。

## 【求职价值】

大消息优化的高频陷阱是把营销术语当端到端保证。岗位需要你列出 copy ledger，并说明硬件缓存一致性和同步成本仍存在。

## Humble 边界

Loaned messages 的可用性依赖 RMW 和消息类型；代码必须先检测能力并提供明确 fallback。新版本能力不反推 Humble。

## 完成定义

- [ ] 能画 10 MB message 的 copy/ownership 路径。
- [ ] 能用 boundedness 解释 loaning 限制。
- [ ] 能给出数据支持的 crossover point，而非宣称 SHM 总是更快。
