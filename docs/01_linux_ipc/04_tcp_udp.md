# Chapter 4 — TCP、UDP 与机器人数据流

重要度：⭐⭐⭐

## 1. 为什么必须同时理解 TCP 和 UDP

DDS/RTPS 的常见网络承载是 UDP，但“机器人实时通信偏向 UDP”不能简化为“UDP 快”。真正的理由涉及：

- 是否允许丢弃旧样本；
- 是否需要 multicast/discovery；
- 丢一个 packet 是否应阻塞后续独立数据；
- reliability 应由整个连接还是由每个 data writer/sample 控制；
- application 是否愿意自己处理 fragmentation、ordering 和 loss；
- 网络、消息大小和安全要求。

TCP 与 UDP 都是工具，不能按“可靠=好、不可靠=坏”排序。

## 2. 生动例子

- TCP 像一条有编号、缺页必补的长卷纸：读者看到连续字节；中间缺一段时，后面的字节即使已到也不能越过缺口交付给应用。
- UDP 像一叠独立明信片：每张边界清楚，可能丢、重复、乱序；新明信片不必等待旧明信片重寄。
- DDS/RTPS 像在明信片之上建立机器人实验室的名录、topic、type、history 和按 writer-reader 关系配置的重发规则。

## 3. 核心理论

### 3.1 TCP

TCP 提供双向、可靠、有序的 byte stream。它不保留 application message boundary：

- 一次 `send(32)`，对端可能 `recv` 为 10 + 22；
- 两次 `send(32)`，对端可能一次 `recv(64)`；
- 应用必须使用 fixed-size、delimiter 或 length-prefix framing。

TCP 通过 sequence/ACK/retransmission/congestion control 实现可靠性。丢失的字节会导致后续 stream data 暂时不能交给应用，即 head-of-line blocking。连接还具有建立、关闭和状态机。

### 3.2 UDP

UDP 是 connectionless datagram：

- 一次 sendto 对应一个 datagram boundary；
- 尽力交付，不承诺到达、顺序、唯一性；
- receiver 未监听时 sender 的成功通常只表示本地内核接受了 datagram；
- 大 datagram 可能发生 IP fragmentation；任一 fragment 丢失可使整个 datagram 不可用；
- 没有 TCP 式 connection-wide congestion/retransmission。

UDP “不可靠”不代表 DDS BEST_EFFORT 与 RELIABLE 都不可靠。DDS 可以在 UDP 上通过 RTPS HEARTBEAT/ACKNACK 与 sample history 实现选择性可靠行为。

### 3.3 Buffer 与 blocking

TCP/UDP 都有 send/receive socket buffer。`send` 成功通常只代表数据被本机 socket 层接受，不证明：

- NIC 已发出；
- 远端内核已收到；
- DDS reader 已入 history；
- ROS subscription 已 take；
- callback 已执行。

blocking 与 non-blocking 改变本地 API 在资源不足时的行为，不改变网络本身的物理容量。

## 4. ROS2/DDS 中如何映射

DDS 为每个 DataWriter/DataReader 建立独立的 endpoint 与 QoS 语义。基于 UDP 的 RTPS 可以：

- 用 multicast 辅助 discovery；
- 用 unicast/multicast transport 发 user data；
- 对 RELIABLE reader 维护 heartbeat/acknack/retransmit；
- 对 BEST_EFFORT sensor stream 允许旧 sample 丢失；
- 用 DATA_FRAG 处理超过 RTPS fragment size 的 sample。

因此“DDS=UDP”也不准确：DDS 是更高层的 data-centric middleware，并可用 shared memory、TCP 或其他 vendor transport。

## 5. Linux 底层发生了什么

本实验固定监听 loopback：

```text
TCP: 127.0.0.1:39001
UDP: 127.0.0.1:39002
```

loopback 仍走 Linux socket/IP stack，只是不经过物理 NIC。两种实验使用同一 32-byte wire format：

- TCP receiver 用 `read_exact` 建立 framing；
- UDP receiver 要求一个 datagram 恰好包含一条 wire message；
- receiver 根据 sequence 统计 gap；
- sender timestamp 和 receiver timestamp 都是同机 monotonic clock。

TCP publisher 带有限时 connect retry，是为了让启动顺序错误产生可理解诊断，而不是无限重试掩盖故障。

## 6. 动手实验

### 实验猜想 A：消息边界

若 sender 连续发送两条 32-byte message，TCP receiver 的两次内核 recv 是否必然各 32 bytes？UDP receiver 呢？

应用层最终都打印两条，但 TCP 的系统调用切分不受 send 次数约束；UDP 保留 datagram boundary。用 strace 修改 buffer/count 验证。

### TCP

终端 A：

```bash
build/stage1/tcp_subscriber --count 10 --port 39001
```

终端 B：

```bash
build/stage1/tcp_publisher --count 10 --interval-ms 20 --port 39001
```

### UDP

终端 A：

```bash
build/stage1/udp_subscriber --count 10 --port 39002
```

终端 B：

```bash
build/stage1/udp_publisher --count 10 --interval-ms 20 --port 39002
```

预期两端都得到 `samples=10 gaps=0`。这只是无拥塞 loopback baseline，不能据此证明 UDP 在真实网络不丢。

### 实验猜想 B：先发后收

先启动 UDP publisher，再启动 subscriber。旧 datagram 会等 subscriber 吗？通常不会；没有 bind 的 receiver queue 供它等待。TCP publisher 则必须建立连接，实验在 deadline 内 retry，超时明确失败。

### 实验猜想 C：慢消费者

给 subscriber 加延迟（可自行在 receive loop 中临时增加 sleep），逐步提高 sender rate：

- TCP：kernel buffers 变满后，backpressure 可能传到 sender，延迟累积；
- UDP：sender 可能继续，receiver queue overflow 时 datagram 被丢，sequence gap 出现；
- 实际阈值取决于 buffer、scheduler、payload 和内核。

## 7. 如何观察系统

### Socket 状态

```bash
ss -ltnp 'sport = :39001'
ss -tnp '( sport = :39001 or dport = :39001 )'
ss -lunp 'sport = :39002'
```

`Recv-Q/Send-Q` 是重要线索，但含义依 socket 类型/状态而异，不能直接称为“ROS QoS queue”。

### Strace

```bash
strace -ff -ttT -e trace=network,read,write,close \
  build/stage1/tcp_subscriber --count 1

strace -ff -ttT -e trace=network,close \
  build/stage1/udp_subscriber --count 1
```

### Tcpdump

```bash
sudo tcpdump -i lo -nn -tttt 'tcp port 39001'
sudo tcpdump -i lo -nn -tttt 'udp port 39002'
```

TCP 能看到 handshake、data、ACK、FIN/RST；UDP 没有 handshake。抓包 timestamp 和应用 `CLOCK_MONOTONIC` 不一定同一 clock/domain，不可直接相减，后续 tracing 章节专门校准。

### 缓冲与统计

```bash
sysctl net.core.rmem_default net.core.rmem_max
sysctl net.core.wmem_default net.core.wmem_max
nstat -az | rg 'Udp|Tcp'
```

统计是系统级累计值。实验前后做差，并避免其他负载污染。

## 8. 常见坑

- 把 TCP package 当 message；
- 只调用一次 read/write；
- 认为 send 成功等于 callback 成功；
- 认为 UDP 没连接就没有内核状态/queue；
- 用 60 KB UDP baseline 推断 5 MB DDS sample 行为；
- 忽略 MTU、fragment loss amplification 和 socket buffer；
- 认为 UDP 一定比 TCP latency 低；
- 认为 robot control 命令用 RELIABLE 就必然安全，忽略 freshness；
- 同机 latency 用 realtime clock，受到 NTP 调整；
- 用 `netstat` 看不到就判断没通信：短连接可能早已消失。

## 9. 真实机器人案例

- **Camera/LiDAR**：高频、新数据替代旧数据，常偏向 BEST_EFFORT，避免重传旧帧造成 backlog；但关键元数据可能不同。
- **Map/configuration**：低频且 late joiner 需要历史，reliability/durability 更重要。
- **Control command**：既关心到达，也关心 freshness、deadline 和 fail-safe；不能只看 reliability。
- **日志/云遥测**：跨 WAN、防火墙和 broker 运维可能更适合 TCP/WebSocket/MQTT/Zenoh route。

**【求职价值】** “为什么机器人偏向 UDP”是高频追问。合格答案必须谈数据 freshness、head-of-line blocking、fragmentation 和 DDS 按 endpoint 提供的可靠性，而不是只说 UDP 快。

## 10. 面试题

1. TCP 为什么需要 framing？
2. UDP 为什么适合一部分实时 sensor data？
3. UDP 上如何实现 DDS RELIABLE？
4. send 成功能保证什么？
5. 为什么大 UDP datagram 在有丢包链路上风险更大？
6. TCP head-of-line blocking 与 ROS2 freshness 有什么关系？

## 11. 🔎 Source Dive 与本章总结

阅读 `man 7 tcp`、`man 7 udp`、`man 7 socket`、`man 2 send`、`man 2 recv`。用 Wireshark 理解 TCP/UDP 后，再在 Stage 03 展开 RTPS submessages。

> TCP 提供可靠有序字节流，UDP 提供独立尽力数据报；DDS 在 transport 之上加入发现、类型、history 和按 endpoint 配置的 QoS。机器人偏向 UDP 的条件是“允许按样本/端点管理 freshness 与可靠性”，不是“UDP 永远更快”。
