# Stage 01 — 进程之间到底怎样通信

重要度：⭐⭐⭐　建议学习时间：12～18 小时

这一阶段刻意少讲 ROS2。先把“地址空间、系统调用、内核缓冲、消息边界、同步与调度”变成可以观察的事实；否则到了 DDS，很容易把多层行为都归因于“ROS2 黑盒”。

## 1. 阶段核心问题

假设两个可执行程序各自有：

```cpp
SensorData sensor;
```

为什么 A 不能把 `&sensor` 这个地址发送给 B，让 B 直接解引用？如果要传递数据，可以：

- 让内核在 pipe/socket buffer 中暂存字节；
- 让网络协议栈跨机器传递 packet；
- 让两个进程显式映射同一组物理页；
- 或者根本不跨进程，把组件放在同一进程，用线程共享对象。

这些选择改变 isolation、copy、syscall、synchronization、failure domain 和可运维性。

## 2. 先建立一个不完美但有用的类比

- **Thread** 像同一办公室里的同事：能看到同一个白板，但同时写必须协调；
- **Process** 像不同门禁房间：门牌号看似一样，不代表房内桌子是同一张；
- **Pipe/socket** 像把文件交给前台（内核），前台再交给另一个房间；
- **Shared memory** 像两个房间开了一扇通往同一仓库的门：少搬一次货，但必须规定谁能改、何时归还；
- **Kernel scheduler** 像值班经理：数据到了不代表接收者立刻获得 CPU。

类比只帮助建立直觉。工程术语是：virtual address space、page table、copy-on-write、file descriptor、system call、kernel buffer、mmap、synchronization、cache coherence、context switch。

## 3. 实验程序

源码包：[`linux_ipc_lab`](../../labs/01_linux_ipc/linux_ipc_lab/README.md)

| Executable | 机制 | 关键观察 |
|---|---|---|
| `thread_shared_state` | 两线程共享对象 | 相同地址、不同 TID、互斥保护 |
| `process_memory_isolation` | `fork` + COW | 相同虚拟地址字符串、修改互不影响 |
| `pipe_process_a` / `pipe_process_b` | anonymous pipe + exec | fd 继承、字节流、EOF |
| `unix_domain_subscriber/publisher` | AF_UNIX datagram | 本机地址、消息边界、kernel queue |
| `tcp_subscriber/publisher` | TCP loopback | connect/accept、stream framing |
| `udp_subscriber/publisher` | UDP loopback | datagram、sequence gap、无连接 |
| `shared_memory_writer/reader` | POSIX shm + process-shared pthread sync | 同一映射、通知、ownership、cleanup |

所有 wire 示例都发送同一个逻辑样本：

```cpp
struct SensorData {
  std::uint64_t timestamp_ns;
  float x;
  float y;
  float z;
};
```

但 pipe/socket 不直接发送这个 native struct。实验定义 32-byte wire format：magic、version、sequence、monotonic timestamp、三个 IEEE-754 float，并显式采用 network byte order。这样 ABI padding 和 endianness 不会偷偷成为协议。

Shared memory 的两端运行在同一主机、共享同一 ABI，直接访问 region 内的 `SensorData`，但使用 process-shared mutex/condition variable 保护 publication。这正好暴露两条路径的设计差异。

## 4. 执行顺序

```bash
./tools/check_environment.sh
./scripts/build_stage1.sh

build/stage1/thread_shared_state
build/stage1/process_memory_isolation
build/stage1/pipe_process_a

./scripts/run_stage1_smoke.sh
```

不要一开始只跑 smoke test。前三个程序逐个运行并观察 PID/TID/address，然后再用脚本验证所有 client/server 对。

## 5. 阶段实验法

每个实验遵循：

1. **猜想**：在运行前写下“谁会阻塞、何时唤醒、输出顺序如何”；
2. **运行**：只改变一个变量；
3. **观察**：stdout 之外至少使用一个 OS 工具；
4. **反证**：寻找能够推翻自己解释的证据；
5. **映射**：指出这个机制在 ROS2/DDS 哪一层会再次出现。

例如，在 pipe 实验前回答：

> 如果 parent 忘记关闭自己的 write end，即使 child 已经读完所有 message，child 能否读到 EOF？

答案不是“看运气”：只要任意进程仍持有该 pipe 的 write end，reader 就不能得到 EOF。这个生命周期错误与 DDS entity、socket、guard condition 的资源管理思维相通。

## 6. 阶段完成标准

### Explain

- [ ] 能解释同一个虚拟地址为何不等于同一物理对象。
- [ ] 能说明 pipe/TCP 是 stream，而 UDP/UDS datagram 保留 record boundary。
- [ ] 能说明 shared memory 快的来源和仍存在的成本。

### Observe

- [ ] 用 `ps -eLf` 找到 thread；
- [ ] 用 `/proc/PID/maps` 找到映射；
- [ ] 用 `strace -ff` 看到 `fork/exec/read/write/send/recv/mmap/futex`；
- [ ] 用 `ss`/`lsof` 看到 socket；
- [ ] 能解释 tcpdump 为什么看不到 pipe/UDS。

### Modify

- [ ] 改大 count/payload，观察 syscall 与 CPU；
- [ ] 让 subscriber 变慢，观察 queue/backpressure；
- [ ] 缩小 socket buffer或 pipe size，观察发送端行为；
- [ ] 故意移除一种同步，能够解释 data race/tearing 风险（不要把错误版本提交为默认代码）。

### Diagnose

- [ ] client/server 启动顺序错误时能从 `errno`、socket 状态定位；
- [ ] TCP partial read 时不会误认为一条 message 丢失；
- [ ] shared-memory stale object/permission 问题能安全清理；
- [ ] 超时能区分“对端没启动”“数据未到”“等 EOF”。

## 7. 【求职价值】

中间件面试常从 `publish()` 一路追问到 `sendto()`、socket buffer、调度，再转向 shared memory 和 zero copy。如果 Linux 模型不稳，答案会出现三类硬伤：

- 把线程共享内存与跨进程 shared-memory transport 混为一谈；
- 把一次 `write/send` 与一次 `read/recv` 一一对应；
- 只说“共享内存快”，说不清同步、ownership、cache coherence 和 crash cleanup。

完成本阶段后，你应能从系统调用和数据所有权讲清这些边界。这也是机器人中间件工程师区别于普通 ROS API 使用者的基础。

## 8. 章节导航

- [01 Process、Thread、虚拟内存](01_process_thread_memory.md)
- [02 Pipe](02_pipe.md)
- [03 Unix Domain Socket](03_unix_domain_socket.md)
- [04 TCP、UDP](04_tcp_udp.md)
- [05 Shared Memory](05_shared_memory.md)
- [06 观察工具与选型](06_observation_and_comparison.md)
- [07 完整实验手册](07_lab_manual.md)
- [Stage 01 面试题](../../interview/stage1_linux_ipc.md)
