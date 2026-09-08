# Chapter 3 — Unix Domain Socket：本机有地址的进程通信

重要度：⭐⭐

> **初学者读法**：先读“0. 先记三件事”“2. 生动例子”和“6. 动手实验”。这一章是从 pipe 过渡到网络 socket 的桥梁；第一遍不需要掌握 `sockaddr_un`、`SCM_RIGHTS` 等接口细节。陌生词见[术语表](../00_glossary.md)。

## 0. 这一章先记三件事

1. Unix Domain Socket（简称 UDS）让**同一台机器上、彼此独立启动**的进程通过一个本机地址找到对方。
2. 它使用 socket 接口，但数据不经过 IP 路由和物理网卡。
3. 本实验采用数据报模式：每次发送的一条数据仍是一条完整记录，但接收队列容量有限，所以不能理解成“永远不丢”。

和 pipe 的关键区别是：pipe 常由父进程把通道交给子进程；UDS 有地址，两个没有亲缘关系的进程也能会合。

## 1. 为什么需要 UDS

匿名 pipe 适合有父子关系、能够继承 fd 的进程。独立启动的后台服务（daemon）和客户端（client）则需要一个双方都知道的本机地址。Unix Domain Socket（AF_UNIX/AF_LOCAL）提供了这种能力，但不会把数据送入 IP 网络。

## 2. 生动例子

Pipe 像两间相邻房之间预埋的管道；UDS 像楼内有房间号的收发室。独立进程知道 pathname 后就能投递，不必由共同 parent 传递 fd。

pathname 是地址，不是把 payload 写进普通磁盘文件。

## 3. 核心理论

UDS 支持：

- `SOCK_STREAM`：类似 TCP 的可靠有序字节流；
- `SOCK_DGRAM`：保留 datagram boundary；
- `SOCK_SEQPACKET`：可靠、有序并保留 record boundary（平台支持时）。

本实验选 AF_UNIX + `SOCK_DGRAM`，因为能清晰对照 UDP datagram，同时避免把 client connection 管理混入第一个 UDS 实验。

pathname socket 的生命周期：

1. receiver `socket`；
2. 清理只属于本实验的 stale pathname；
3. `bind` 到 `sockaddr_un.sun_path`；
4. sender `sendto` 该地址；
5. receiver `recvfrom`；
6. receiver 关闭 fd 并 `unlink` pathname。

`unlink` 删除名字，不会把已经打开的 socket fd 变成普通文件。若进程崩溃，pathname 可能残留；下次 `bind` 会得到 `EADDRINUSE`，但盲删非本进程拥有的路径会破坏别的服务。

## 4. ROS2 中如何实现/为何不只用 UDS

UDS 很适合同机服务，但 ROS2 的目标包含：

- 同一 API 跨进程、跨主机；
- distributed discovery 与动态 endpoint；
- typed topic 和 QoS；
- 一对多、多对多；
- vendor-neutral RMW；
- UDP、SHM 等 transport 选择。

“ROS2 为什么不简单用 Unix Socket”的准确回答不是 UDS 性能差，而是它只解决本机字节/record transport；DDS/RMW 还解决数据模型、发现、匹配、QoS、history 和跨网络互操作。

有些 middleware/bridge/daemon 内部仍可能使用 UDS。是否使用要看具体实现，不能从 ROS2 API 推断。

## 5. Linux 底层发生了什么

`unix_domain_subscriber` bind 默认路径 `/tmp/ros2_comm_lab_uds_<uid>.sock`。加入 UID 避免不同用户冲突；程序用 RAII 在正常/异常退出时清理自己成功 bind 的 pathname。

`unix_domain_publisher` 将统一 wire message 通过 `sendto` 发送。每次 `recvfrom` 得到一个 datagram；若接收 buffer 小于 datagram，超出部分会被截断而不是留给下一次 recv。本实验要求长度恰好等于协议长度，长度不对立即报错。

数据仍会从 sender user buffer 复制/传入 kernel socket queue，再到 receiver user buffer；但没有 IP routing、IP header 或 NIC。

## 6. 动手实验

### 实验猜想

1. publisher 在 subscriber bind 前发送，会排队等待还是立即失败？
2. 一次 sendto 两个样本拼在同一 datagram，receiver 能否自动拆成两条协议消息？
3. 在物理网卡抓包能看到 UDS payload 吗？
4. receiver sleep 后，队列满时 sender 会发生什么？

### 运行

终端 A：

```bash
build/stage1/unix_domain_subscriber --count 10
```

终端 B：

```bash
build/stage1/unix_domain_publisher --count 10 --interval-ms 20
```

预期：

```text
uds_subscriber path=/tmp/ros2_comm_lab_uds_1000.sock count=10
uds_rx seq=0 xyz=(0,...,...) latency_us=...
...
uds_subscriber complete samples=10 gaps=0
```

latency 使用同机 `CLOCK_MONOTONIC`。它包含 sendto、排队、调度与 recv/decode，不代表跨机可比的绝对网络 latency。

## 7. 如何观察系统

subscriber 运行时：

```bash
ss -xap | rg ros2_comm_lab_uds
lsof -U | rg ros2_comm_lab_uds
ls -l "/tmp/ros2_comm_lab_uds_$(id -u).sock"
```

系统调用：

```bash
strace -ff -ttT \
  -e trace=socket,bind,sendto,recvfrom,close,unlink \
  build/stage1/unix_domain_subscriber --count 1
```

另一个终端发送一条。`tcpdump -i any` 不会显示 AF_UNIX payload，因为它不走 packet capture 的 IP/Ethernet 路径。strace、ss 和 lsof 才是合适观察面。

## 8. 常见坑

- `sun_path` 超长而被截断/拒绝；
- stale pathname 与活跃服务无法仅凭“文件存在”区分；
- 目录权限导致 bind/connect 失败；
- 认为 datagram 就永不丢：receiver queue 仍有限；
- 把 UDS pathname 当普通数据文件；
- 在 signal handler 里执行不安全的复杂清理；
- 直接发送 C++ object 指针。

本实验的自动化脚本使用唯一临时路径并显式传给两端，避免并行测试互相删除 socket。

## 9. 真实机器人案例

本机 telemetry agent、日志 daemon、硬件安全服务或容器 sidecar 可以用 UDS 暴露接口。对于相机帧，UDS 可以传 fd（`SCM_RIGHTS`）以共享 DMA/memfd buffer，而不是复制整帧；这已经是更复杂的 ownership 协议，后续 zero-copy 章节会用相同思维分析。

**【求职价值】** UDS 常用于机器人本机 daemon、日志与硬件服务。面试重点不是会调用 socket，而是能解释 pathname lifecycle、credential/fd passing 和它为何不等于 DDS。

## 10. 面试题

1. UDS 和 TCP loopback 的主要区别是什么？
2. UDS datagram 是否可靠？
3. pathname 残留怎么安全处理？
4. 为什么 UDS 不等于 shared memory？
5. ROS2 为什么不把 UDS 当唯一通信机制？

## 11. 🔎 Source Dive 与本章总结

阅读 `man 7 unix`、`man 2 socket`、`man 2 bind`、`man 2 sendto`、`man 2 recvfrom`。扩展阅读 `SCM_RIGHTS` 前，先画清 fd 与 fd 所引用内核对象的区别。

> UDS 是本机 socket IPC：有地址、有内核队列、可选择 stream/datagram 语义，但不提供 DDS 的发现、类型、QoS 和分布式数据模型。

第一遍可以把它说成：

> UDS 像一间只有本楼能使用、带房间号的收发室。它解决“本机两个独立程序怎样找到并传数据”，但没有替 ROS 2 解决跨机器、自动发现和通信策略等更大问题。
