# ROS2 Communication Big Picture

版本：v0.1（Stage 01 建立 Linux 地基；后续阶段逐层补充真实 trace、vendor 差异和源码入口）

这张图不是“组件清单”，而是一张责任、线程、队列和延迟地图。读图时始终问：

1. 当前数据由谁拥有？
2. 当前代码运行在哪个线程？
3. 跨越用户态/内核态或进程边界了吗？
4. 哪里有 queue/history/cache？
5. 谁使下一层变成 ready，又是谁选择 callback？

## 1. 系统全景

```text
Laptop A / Process P                                      Laptop B / Process Q
┌──────────────────────────────────────┐                 ┌──────────────────────────────────────┐
│ Application                          │                 │ Application                          │
│ ROS message + publisher_->publish()  │                 │ user subscription callback           │
├──────────────────────────────────────┤                 ├──────────────────────────────────────┤
│ rclcpp                               │                 │ rclcpp Executor                      │
│ typed API / intra-process policy     │                 │ CallbackGroup admission + dispatch   │
├──────────────────────────────────────┤                 ├──────────────────────────────────────┤
│ rcl                                  │                 │ rcl WaitSet / take                   │
│ C lifecycle and client abstraction   │                 │ readiness abstraction                │
├──────────────────────────────────────┤                 ├──────────────────────────────────────┤
│ rmw implementation                   │                 │ rmw implementation                   │
│ vendor-neutral ROS middleware API    │                 │ DDS reader readiness / take          │
├──────────────────────────────────────┤                 ├──────────────────────────────────────┤
│ DDS implementation                   │                 │ DDS implementation                   │
│ DataWriter history / QoS / discovery │                 │ DataReader history / QoS             │
│ CDR serialization / RTPS writer      │                 │ RTPS reader / deserialization        │
├──────────────────────────────────────┤                 ├──────────────────────────────────────┤
│ transport                            │                 │ transport                            │
│ UDP socket or shared-memory segment  │                 │ UDP socket or shared-memory segment  │
└──────────────┬───────────────────────┘                 └─────────────────▲────────────────────┘
               │ user/kernel boundary                                      │
┌──────────────▼───────────────────────┐                 ┌─────────────────┴────────────────────┐
│ Linux kernel                         │                 │ Linux kernel                         │
│ socket buffers / IP / UDP / qdisc    │                 │ driver / IP / UDP / socket buffers  │
│ scheduler / virtual memory / futex   │                 │ scheduler / virtual memory / futex   │
└──────────────┬───────────────────────┘                 └─────────────────▲────────────────────┘
               │ NIC / Ethernet / Wi-Fi                                    │
               └────────────────── physical network ────────────────────────┘
```

**重要修正**：DDS 不是简单的“ROS2 网络层”。DDS 是以数据为中心的 pub/sub 规范族，包含实体模型、发现、匹配、QoS 和数据分发语义；RTPS 是常见的互操作 wire protocol；UDP 或 shared memory 才更接近 transport。

同样不要把图中的每一行理解为对象一一对应。ROS publisher/subscription 通常由 RMW 落实为 DDS DataWriter/DataReader，但 ROS Node 与 DDS DomainParticipant 是否一一对应不是 ROS2 API 的保证；RMW 实现可按 context/domain 复用 participant。后续源码和抓包必须以所选 Humble RMW 为准。

## 2. 发布侧：从一次函数调用到 transport

```text
[P0] construct/fill ROS message
  │  allocation, cache misses, producer lock
  ▼
[P1] rclcpp publish path
  │  intra-process and inter-process decisions
  ▼
[P2] rcl_publish → rmw_publish
  │  type support dispatch, RMW/vendor boundary
  ▼
[P3] DDS DataWriter
  │  serialize, history-cache insertion, QoS/resource limits
  ▼
[P4] RTPS writer / transport
  │  fragmentation, reliability bookkeeping, send queue
  ▼
[P5] syscall + kernel
  │  socket buffer, protocol stack, qdisc, scheduler
  ▼
[P6] NIC / network
```

`publish()` 返回并不普遍等于“远端 callback 已执行”，甚至不一定等于“数据已经离开网卡”。同步/异步 publish 模式、DDS vendor、QoS 和 resource limits 会改变返回点附近的工作和阻塞行为；后续实验必须声明具体配置。

## 3. 订阅侧：数据到达不等于 callback 立即执行

```text
NIC / shared-memory notification
  ▼
[S0] kernel receive path or shared segment
  ▼
[S1] DDS transport/receive worker thread
  │  RTPS parse, fragment reassembly, reliability, reader history
  ▼
[S2] rmw/rcl entity becomes ready
  │
  ▼
[S3] WaitSet wakes
  │
  ▼
[S4] Executor thread selects executable
  │  thread availability + CallbackGroup rules
  ▼
[S5] take / deserialize (exact placement is RMW/vendor/path dependent)
  │
  ▼
[S6] user callback start → work → callback end
```

普通 `rclcpp` subscription callback 通常不是由 DDS 接收线程直接调用。DDS/RMW 使 subscription 可取，Executor 基于 WaitSet 醒来并调度 callback。精确的 take/deserialization 时点与路径在 Stage 04/06 用 Humble 源码和 tracing 验证。

## 4. 三类线程不要混淆

| 线程类别 | 典型职责 | 是否通常运行用户 callback |
|---|---|---:|
| 应用/Executor 线程 | wait、选择 executable、执行 timer/subscription/service callback | 是 |
| DDS 内部线程 | discovery、接收、事件、可靠性、异步发送 | 通常否 |
| 内核线程/中断上下文 | NIC、网络栈、调度与内存管理的一部分 | 否 |

MultiThreadedExecutor 增加的是 Executor worker 并发机会，不等于 DDS receive thread 数量；CallbackGroup 还会限制哪些 callback 能同时进入。

## 5. 队列与缓存地图

```text
producer-owned buffer
      │
      ▼
DDS writer history cache     ← history/depth/resource limits/durability
      │
      ▼
transport/send queues        ← async publish, socket send buffer
      │
      ▼
network/qdisc/NIC queues     ← bandwidth, MTU, congestion, Wi-Fi retries
      │
      ▼
socket receive buffer
      │
      ▼
DDS reader history cache     ← reliability/history/depth/lifespan
      │
      ▼
Executor-ready work          ← thread busy, callback group, scheduling
      │
      ▼
application's own queue      ← optional worker queue, lock/backpressure
```

“队列长度 10”不能在没有层次限定时使用。ROS QoS depth 主要对应 DDS history 语义，不是 Linux UDP socket 的十个 packet，也不是 Executor 明确维护的十个 FIFO callback。

## 6. 延迟预算地图

端到端 message age 可近似拆解为：

```text
T_end_to_end
  = T_application_prepare
  + T_publish_api
  + T_serialize_and_writer_cache
  + T_transport_queue
  + T_kernel_and_network
  + T_receive_and_reader_cache
  + T_executor_scheduling
  + T_deserialize_or_take
  + T_callback_work
```

抖动是这些项随时间变化后的合成结果。常见来源：

- CPU 抢占、page fault、频率调节、IRQ；
- allocator 和大块内存复制；
- DDS batching、fragmentation、retransmission、heartbeat；
- socket buffer、qdisc、Wi-Fi 重传；
- history backlog 和旧消息累积；
- Executor 线程忙、CallbackGroup 互斥；
- callback 内阻塞 I/O、锁竞争或日志；
- 时间戳跨机器但时钟未同步。

所以 `now() - msg.header.stamp` 只有在时间基准可信、stamp 语义明确时才叫单向 latency；否则它可能是 clock offset 的测量。

## 7. Discovery 平面与数据平面

```text
Discovery plane:
  SPDP: Participant ↔ Participant
  SEDP: Writer/Reader endpoint metadata exchange
  result: compatible endpoints know locators and QoS

Data plane:
  RTPS DATA / DATA_FRAG / HEARTBEAT / ACKNACK ...
  result: samples and reliability control traffic move between matched endpoints
```

能看到 topic 只证明图发现的某些信息存在；它不能独立证明：

- QoS compatible；
- user-data locator 可达；
- 大样本 fragments 全部到达；
- subscriber 正在 take；
- Executor 有空执行；
- callback 没有被同组长任务阻塞。

## 8. Linux IPC 与 ROS2 的连接

| Stage 01 机制 | 在后续 ROS2 中对应的思考方式 |
|---|---|
| pipe 字节流与 EOF | waitable fd、blocking、生命周期、framing |
| UDS | 本机 IPC 有地址与内核缓冲，但 DDS 不被绑定为单一 IPC |
| TCP stream | reliability/order 与 head-of-line blocking 的取舍 |
| UDP datagram | RTPS 常用承载、丢包、fragmentation、重传策略上移 |
| shared memory | 共享页减少 transport copy，但需要同步、ownership、cleanup |
| thread shared state | intra-process 可以传对象/指针，但并发安全仍是应用责任 |
| process isolation | inter-process 必须借助 kernel/transport/共享映射 |

## 9. 五个容易混淆的概念

| 概念 | 边界 | 不自动意味着 |
|---|---|---|
| Intra-process communication | 同一进程内，rclcpp 可绕开常规 RMW 路径 | 不等于跨进程 SHM |
| Inter-process communication | 不同进程之间 | 不限定 UDP/TCP/SHM |
| Shared-memory transport | 两进程映射同一物理页并通过协议协调 | 不一定端到端零复制 |
| Loaned message | middleware/allocator 借 buffer 给应用填写或读取 | 所有消息/RMW 都支持 |
| Zero copy | 特定路径上避免特定复制 | 不等于零序列化、零同步、零 cache cost |

## 10. 版本和实现边界

本课程所有“默认行为”都必须写成完整句子，例如：

> 在 Ubuntu 22.04、ROS2 Humble、指定版本的 `rmw_fastrtps_cpp` 与给定 XML 配置下观察到……

不能写成：

> ROS2 总是……

Humble 是 LTS，但 vendor 库补丁版本、默认 RMW、容器网络和发行包更新都会影响现象。Rolling/Jazzy 的 executor、loaned message、type adaptation、DDS vendor 版本或默认配置变化，会在相关章节用“**Humble 行为** / **新版本变化**”标出，而不会让新版本代码污染 Humble 基线。

## 11. 白板终局检查

完成课程后，应能在这张图上逐点标出：

- serialization 与 deserialization；
- application/DDS/kernel queues；
- Executor、DDS 与内核线程；
- user/kernel、process、machine 三类边界；
- discovery 与 user data；
- QoS 生效位置；
- context switch 和 wake-up；
- copy、allocation、cache coherence；
- latency/jitter/loss 的证据和观测工具。

每完成一个阶段，都应更新自己的手绘版本，而不是只阅读本文件。
