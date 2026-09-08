# Stage 01 — 进程之间到底怎样通信

位置：[统一路线](../00_course_map.md)顺序 0。完成第一遍后进入 [ROS 2 使用基础](../15_ros2_basics/README.md)；工具深挖和源码可在通信进阶时回来补，不必先完成所有深挖才能继续。

重要度：⭐⭐⭐　初学者第一遍：约 4～6 小时　完整学习：约 12～18 小时

这一阶段刻意少讲 ROS 2。先通过几个小程序看懂“同一进程怎样共享数据、不同进程怎样传数据”。地址空间、系统调用、消息边界等严格术语，会在看到现象后再逐个补上。

如果这些术语大多是第一次见，请先完成[30 分钟初学者入口](../00_beginner_path.md)。阅读中随时查询[术语表](../00_glossary.md)。

## 0. 第一遍只记住这张图

```text
同一进程里的线程
线程 A ─────── 直接访问同一个对象 ─────── 线程 B
                    需要协调同时读写

不同进程
进程 A ──→ pipe / socket 中转 ──→ 进程 B
   └────→ shared memory 共同区域 ─────┘
```

现在只需要理解四个词：

- **进程（process）**：一个正在运行的程序实例，默认有自己的内存视图；
- **线程（thread）**：进程里的一条执行路线，同一进程的线程通常能看到相同对象；
- **IPC**：进程间通信，泛指不同进程交换数据的方法；
- **内核（kernel）**：Linux 的核心管理者，pipe、socket、进程和内存都由它协助管理。

第一遍每章只读“先记三件事”“生动例子”和“动手实验”。“核心理论”“如何观察系统”“Source Dive”和面试题可以留到第二、三遍。

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

这些选择会改变隔离性（一个程序崩溃是否影响另一个）、数据复制次数、等待方式和维护难度。后面再把它们对应到 `isolation`、`copy`、`syscall`、`synchronization` 等工程术语。

## 2. 先建立一个不完美但有用的类比

- **线程（thread）**像同一办公室里的同事：能看到同一个白板，但同时写必须协调；
- **进程（process）**像不同门禁房间：抽屉编号看似一样，不代表房内是同一个抽屉；
- **管道或套接字（pipe/socket）**像把文件交给前台（内核），前台再交给另一个房间；
- **共享内存（shared memory）**像两个房间共用一间仓库：少搬一次货，但必须规定谁能改、何时归还；
- **内核调度器（kernel scheduler）**像值班经理：数据到了，不代表接收者立刻获得 CPU 时间。

类比只帮助建立直觉。`virtual address space`、`copy-on-write`、`file descriptor`、`system call` 等严格说法会在对应章节解释；不用在这里一次记住。

## 3. 实验程序

源码包：[`linux_ipc_lab`](../../labs/01_linux_ipc/linux_ipc_lab/README.md)

| 可执行程序 | 采用的方法 | 第一遍看什么 |
|---|---|---|
| `thread_shared_state` | 两线程共享对象 | PID 相同、TID 不同、对象地址相同 |
| `process_memory_isolation` | 创建子进程 | 地址文字可相同，但修改互不影响 |
| `pipe_process_a` / `pipe_process_b` | 匿名管道 | A 写字节、B 读字节、关闭后出现 EOF |
| `unix_domain_subscriber/publisher` | 本机数据报 socket | 独立启动的程序通过本机路径找到对方 |
| `tcp_subscriber/publisher` | TCP 回环通信 | 连续字节需要由程序重新分出消息 |
| `udp_subscriber/publisher` | UDP 回环通信 | 每次发送保留一条数据报边界，但可能丢失 |
| `shared_memory_writer/reader` | POSIX 共享内存 | 两进程访问共同区域，同时仍要协调读写 |

这些通信示例都发送同一种简化的传感器数据：

```cpp
struct SensorData {
  std::uint64_t timestamp_ns;
  float x;
  float y;
  float z;
};
```

pipe/socket 不会直接发送这个 C++ 对象，而是先把它整理成明确的 32 字节传输格式。第一遍只需知道“发送方和接收方必须约定每个字节代表什么”；magic、ABI、padding、endianness 等细节可在第二遍查询[术语表](../00_glossary.md)。

共享内存示例的两端都在同一台机器上，可以直接访问共同区域中的 `SensorData`，但仍用跨进程锁和通知机制避免一方读到“只写了一半”的数据。这正好展示“搬运数据”和“共享数据”两条路线的差异。

## 4. 三种学习深度

| 路线 | 做什么 | 暂时跳过什么 |
|---|---|---|
| 第一次体验 | 逐个运行前 3 个程序，读每章的类比和白话结论 | 所有观察工具、源码深挖、面试题 |
| 完整学习 | 完成 7 个机制和对应实验，能解释主要现象 | 内核源码和精细性能分析 |
| 工程深挖 | 使用 `strace`、`ss`、`tcpdump`、`perf`，做故障注入和对照实验 | 无，按问题选择深入内容 |

## 5. 执行顺序

```bash
./tools/check_environment.sh
./scripts/build_stage1.sh

build/stage1/thread_shared_state
build/stage1/process_memory_isolation
build/stage1/pipe_process_a

./scripts/run_stage1_smoke.sh
```

不要一开始只跑 smoke test。前三个程序逐个运行并观察 PID/TID/address，然后再用脚本验证所有 client/server 对。

第一次只运行到 `pipe_process_a` 就可以暂停。`run_stage1_smoke.sh` 是学完整个阶段后的综合检查，不是入门门槛。

## 6. 阶段实验法

从第二遍开始，每个实验遵循：

1. **猜想**：在运行前写下“谁会阻塞、何时唤醒、输出顺序如何”；
2. **运行**：只改变一个变量；
3. **观察**：stdout 之外至少使用一个 OS 工具；
4. **反证**：寻找能够推翻自己解释的证据；
5. **映射**：指出这个机制在 ROS2/DDS 哪一层会再次出现。

例如，在 pipe 实验前回答：

> 如果 parent 忘记关闭自己的 write end，即使 child 已经读完所有 message，child 能否读到 EOF？

答案不是“看运气”：只要任意进程仍持有该 pipe 的 write end，reader 就不能得到 EOF。这个生命周期错误与 DDS entity、socket、guard condition 的资源管理思维相通。

## 7. 阶段完成标准

第一次学习只要求完成[初学者入口中的五项检查](../00_beginner_path.md#9-第一次学习完成标准)。下面是完整 Stage 01 的进阶标准。

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

## 8. 【求职价值】

中间件面试常从 `publish()` 一路追问到 `sendto()`、socket buffer、调度，再转向 shared memory 和 zero copy。如果 Linux 模型不稳，答案会出现三类硬伤：

- 把线程共享内存与跨进程 shared-memory transport 混为一谈；
- 把一次 `write/send` 与一次 `read/recv` 一一对应；
- 只说“共享内存快”，说不清同步、ownership、cache coherence 和 crash cleanup。

完成本阶段后，你应能从系统调用和数据所有权讲清这些边界。这也是机器人中间件工程师区别于普通 ROS API 使用者的基础。

## 9. 章节导航

- **必学**：[01 进程、线程与内存](01_process_thread_memory.md)
- **必学**：[02 Pipe（管道）](02_pipe.md)
- **可选**：[03 Unix Domain Socket](03_unix_domain_socket.md)
- **必学**：[04 TCP 与 UDP](04_tcp_udp.md)
- **必学**：[05 Shared Memory（共享内存）](05_shared_memory.md)
- **学完整阶段后阅读**：[06 观察工具与选型](06_observation_and_comparison.md)
- **做实验时按需查询**：[07 完整实验手册](07_lab_manual.md)
- **求职准备时阅读**：[Stage 01 面试题](../../interview/stage1_linux_ipc.md)
