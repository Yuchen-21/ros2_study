# Chapter 6 — 如何观察、比较和选择 IPC

重要度：⭐⭐⭐

## 1. 为什么工具越多不等于证据越强

每个工具观察不同平面：

- `ps/top/perf` 看调度和 CPU；
- `strace` 看用户态跨入内核的 syscall；
- `ss/lsof` 看 socket/fd；
- `tcpdump/Wireshark` 看 packet capture 点的网络包；
- `/proc/PID/maps` 看虚拟内存映射；
- 应用 sequence/timestamp 看业务样本。

用 tcpdump 查 pipe，或用 topic Hz 推断 kernel queue，属于观测面错配。

## 2. 工具地图

| 工具 | 能回答 | 不能单独证明 |
|---|---|---|
| `ps -L` / `top -H` | 有哪些线程、CPU 使用、状态 | callback 因何延迟 |
| `strace -ff -ttT` | 哪个进程何时调用 syscall、阻塞多久 | 用户态内部具体函数耗时 |
| `ss` | TCP/UDP/UDS endpoint、queue、state | DDS history 或 executor queue |
| `lsof` | PID 持有哪些 fd/socket/shm object | payload 内容和 QoS |
| `tcpdump` | capture interface 上的 TCP/UDP/RTPS packet | pipe/UDS payload、远端 callback |
| Wireshark | 协议字段、flow、RTPS submessage | 应用 callback scheduling |
| `perf stat` | context switch、fault、cycles 等计数 | 一条消息的完整因果链 |
| `perf record` | CPU hotspot/call graph | sleep/wait 的全部时序原因 |
| `/proc/PID/maps` | 映射区域与共享对象 | 该页何时被哪核访问 |
| stdout sequence/stamp | sample gap、age、业务事件 | gap 在哪一层发生 |

后续 `ros2_tracing` 把 rclcpp/rcl/rmw callback 生命周期补入这个矩阵。

## 3. 一套分层观察流程

### Step A — 确认进程和线程

```bash
pgrep -a tcp_subscriber
ps -L -p PID -o pid,tid,psr,stat,pcpu,comm
```

问题：进程活着吗？线程在 running、sleeping 还是 uninterruptible wait？

### Step B — 确认 fd 和 endpoint

```bash
lsof -p PID
ss -ltnup
ss -xap
```

问题：是否 bind 到预计地址/端口？TCP 是否 ESTABLISHED？queue 是否增长？

### Step C — 看 syscall 与阻塞

```bash
strace -ff -ttT -p PID -e trace=network,read,write,futex
```

尖括号中的 syscall duration 很长，可能表示 blocking wait；但 futex wait 可能是正常空闲，不等于死锁。

### Step D — 看 packet（只适用 IP 路径）

```bash
sudo tcpdump -i lo -nn -s 0 -w /tmp/ipc_lab.pcap \
  'tcp port 39001 or udp port 39002'
```

问题：packet 到了哪个 interface？方向和时间？有 retransmission/ICMP/fragment 吗？

### Step E — 看应用语义

比较 sequence、source timestamp、receive timestamp。packet 到达而 sequence 未被 callback 处理，才把焦点移向 receive queue、middleware take 和 scheduling。

## 4. IPC 对比：不是一张“速度排行榜”

| 维度 | Pipe | UDS datagram | TCP | UDP | Shared memory |
|---|---|---|---|---|---|
| 范围 | 本机 | 本机 | 本机/跨机 | 本机/跨机 | 通常本机 |
| Endpoint | inherited fd | pathname/abstract | IP:port + connection | IP:port | name + protocol |
| 语义 | ordered byte stream | datagram | reliable ordered stream | best-effort datagram | load/store + 自定义协议 |
| 消息边界 | 无 | 有 | 无 | 有 | 自己设计 |
| 内核缓冲 | pipe buffer | socket queue | socket/protocol buffers | socket queue | backing pages；通知另算 |
| Backpressure | 满时阻塞/EAGAIN | queue limit | flow control/满时阻塞 | drop/EAGAIN 等 | slot/ring policy 自定 |
| Discovery | 无 | 无 | 无 | 无 | 无 |
| Fan-out | 自建 | 自建 | 自建 | multicast/自建 | 自建 reader ownership |
| 大 payload copy 潜力 | 较高 | 较高 | 较高 | 较高 | 可较低 |
| 复杂度 | 低 | 中 | 中 | 中 | 高 |
| Crash cleanup | fd | path | connection state | endpoint state | segment/lock/slot |

“DDS”不应作为与 TCP/UDP 同层的一列来简单比较。DDS 可使用这些 transport 中的若干种，并在上层提供 discovery、typing、QoS、history 和 pub/sub。

## 5. 性能实验怎样做才可信

### 先定义指标

- latency：从哪个 timestamp 到哪个 timestamp？
- throughput：application payload 还是 wire bytes？
- CPU：一个进程、两端之和还是 system？
- memory：RSS、PSS、shared pages 还是 allocator bytes？
- loss：source sequence gap 还是抓包 loss？
- jitter：inter-arrival 的 variation 还是 latency distribution？

### 控制变量

- Release/RelWithDebInfo，不拿 Debug 代表生产性能；
- 相同 payload/count/warm-up；
- 相同 CPU affinity/governor/系统负载；
- 同一机器同一内核；
- 报告 buffer size、blocking mode；
- 重复运行，报告 P50/P95/P99/max 和样本量；
- 先验证正确性，再测速；
- 大消息预分配与否必须声明。

### 建议练习

将 wire payload 扩展为 1 KB/100 KB/1 MB，分别测 UDS/TCP loopback/UDP（注意 UDP 单 datagram 上限与 fragmentation，不要直接发送 1 MB datagram）/SHM。高质量结论应类似：

> 在给定硬件与单 writer/reader、payload X、buffer Y 下，SHM 路径降低了 Z 类 syscall/copy；P99 仍受调度和 lock contention 影响。

而不是：

> SHM 比 UDP 快 N 倍。

Stage 06 会提供统一 benchmark harness。

## 6. 实验猜想

在 publisher 每 1 ms 发送、subscriber 每次处理 10 ms 时：

- TCP 的应用 message age 会如何变化？
- UDP 的 sequence gap 与 age 如何变化？
- single-slot SHM 的 generation gap 如何变化？

直觉目标：

- TCP 更倾向保留 stream，可能形成 backlog/age；
- UDP receiver queue 满后更倾向 drop；
- overwrite-style SHM 若设计为 latest-value，reader 看新值但跳 generation；
- 精确行为仍由 blocking/non-blocking、buffer 和协议策略决定。

这正是 QoS Stage 要讨论的 freshness vs completeness。

## 7. 常见误判

- **CPU 低，所以快**：线程可能阻塞且 latency 很高；
- **Hz 正常，所以无延迟**：稳定输出旧消息也可能 100 Hz；
- **没有 sequence gap，所以网络好**：TCP 重传可保持完整但增加尾延迟；
- **抓到 packet，所以 callback 应执行**：中间还有 reader history、take、executor；
- **sendto 返回成功，所以远端收到**：只说明本地调用成功；
- **RSS 高，所以复制多**：shared pages、allocator cache 与 RSS 口径要拆；
- **strace 慢，所以程序慢**：strace 自身会有显著扰动；
- **loopback 结果代表网卡**：loopback 不经过物理 NIC/driver/link。

## 8. 真实机器人选型框架

每条数据流填写：

| 问题 | 示例 |
|---|---|
| scope | 同线程、同进程、同机、跨机、WAN？ |
| size/rate | 6 MB × 30 FPS，还是 64 B × 100 Hz？ |
| freshness | 旧数据还有价值吗？ |
| completeness | 能否丢样本？可丢多少？ |
| fan-out | 一个还是多个 reader？ |
| isolation | producer 崩溃能否带走 consumer？ |
| backpressure | 慢 reader 阻塞、丢旧还是断开？ |
| ownership | buffer 何时可复用？ |
| observability | 如何看到 queue、loss、age？ |
| deployment | 容器、权限、跨 VLAN/Wi-Fi？ |

然后选择“同进程 ownership transfer / DDS UDP / DDS SHM / bridge / CAN”等，而不是全系统只有一种总线。

## 9. 【求职价值】

面试中的“ROS2 为什么有延迟”不是让你列名词。建议按以下顺序回答：

1. 先定义从哪里到哪里的 latency；
2. 画出 producer、serialization/history、transport/kernel/network、reader、executor/callback；
3. 标出每个 queue/thread/copy；
4. 为每层给一个观测工具；
5. 用控制变量实验缩小范围；
6. 最后给优化，不先猜参数。

## 10. 面试题

1. 如何证明一个 socket send 正在阻塞？
2. tcpdump 看见 packet 后，为什么 callback 仍可能不运行？
3. 如何公平比较 UDS、UDP loopback 和 shared memory？
4. latency、inter-arrival 与 callback duration 有什么区别？
5. 为什么 `ros2 topic hz` 未来只能作为一个观测面？

## 11. 本章总结

> 工具只在自己的观测平面内提供证据。IPC 选型取决于范围、语义、数据规模、freshness、隔离、ownership 和故障恢复，不存在脱离条件的单一“最快中间件”。
