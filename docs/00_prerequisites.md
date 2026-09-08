# 前置知识与环境自检

这不是准入考试，也不是要求全部打勾后才能开始的清单。第一次学习 Stage 01，只要会在终端进入目录、运行命令，并能大致阅读简单 C++ 即可。其余内容本来就应该在实验中逐步学会。

如果下面大部分词都是第一次见，请先去[初学者入口](00_beginner_path.md)，不要从这份长清单开始。陌生词可在[术语表](00_glossary.md)中查询。

## 0. 怎样使用这份清单

| 层次 | 怎样处理 |
|---|---|
| 起步所需 | 会打开终端、使用 `cd`、复制命令并看输出末尾是否报错；系统有 `g++`、`cmake` 和 `make` |
| Stage 01 内学习 | 进程/线程、fd、阻塞、socket、共享内存等，不会正是来学习的原因 |
| 后续或深挖 | ROS 2、DDS、tracing、memory order、性能工具等，第一遍可以跳过 |

勾不上的项目只表示“这里可能需要慢一点或查一下”，不表示不合格。

## 1. C++17：先能读一点，边做边补

### 第一遍最好大致认识

- [ ] 值、引用、指针、`const` 的区别；
- [ ] 栈对象析构与 RAII；
- [ ] `std::unique_ptr`/`std::shared_ptr` 的所有权含义；
- [ ] `std::thread`、`std::mutex`、`std::condition_variable`；
- [ ] `std::chrono::steady_clock`；
- [ ] 异常与返回码的边界；
- [ ] 模板类的基本阅读能力。

### 第二遍或后续阶段再补

- [ ] move semantics；
- [ ] `std::atomic` 和 memory order 的目的；
- [ ] alignment、padding、trivially copyable；
- [ ] 虚函数、type erasure 和 shared library symbol；
- [ ] CMake target-based 写法。

**进阶检查题（第一遍可跳过）**：为什么不能把含有 `std::string` 的 C++ struct 直接 `send(fd, &object, sizeof(object), ...)` 给另一个进程？

合格回答应包含：对象内是指针/容量等进程内状态；虚拟地址对另一个进程无意义；ABI、padding、字节序与版本也不稳定；需要定义 wire format 和序列化。

## 2. Linux：这些是 Stage 01 的学习目标

下面列的是学完第一阶段后希望逐渐掌握的能力，不是开始前的要求：

- [ ] 能解释 PID、TID、文件描述符；
- [ ] 知道系统调用为何从用户态进入内核态；
- [ ] 会读 `ps -eLf`、`top -H`、`/proc/PID/status`；
- [ ] 理解 blocking、non-blocking、timeout；
- [ ] 知道 `SIGINT` 与进程退出的关系；
- [ ] 会使用 `ip addr`、`ip route`、`ss`；
- [ ] 知道端口、IP 地址、loopback 和 NIC 的区别；
- [ ] 能用 `strace` 观察系统调用；
- [ ] 知道 root/capability 为什么影响 `tcpdump`、`tc`、network namespace。

**进阶检查题（学 pipe 时再看）**：`write(fd, buffer, 1000)` 返回 400 是否一定是错误？

不是。字节流/非阻塞 I/O 可能发生 partial write；程序要按已写字节推进。实验的 `write_all` 正是为这个语义存在。对 datagram，partial datagram 的处理语义不同，不能套用字节流循环。

## 3. 网络：学 TCP/UDP 前后各看一次

- [ ] 分清 Ethernet frame、IP packet、UDP datagram、TCP byte stream；
- [ ] 理解 MTU 与 IP fragmentation 的基本关系；
- [ ] 理解 TCP reliability 不等于应用实时性；
- [ ] 知道 multicast 与 broadcast/unicast 的区别；
- [ ] 知道 socket send buffer 和 receive buffer 会排队；
- [ ] 能解释 packet loss、latency、jitter、reordering；
- [ ] 对抓包过滤表达式有基本认识。

**进阶检查题（学 TCP/UDP 时再看）**：为什么同机的 `127.0.0.1` UDP 也不是“直接函数调用”？

它仍经过 socket API、内核网络栈、socket buffer 和调度；只是不会从物理 NIC 发出。抓 `lo` 能看到，抓物理网卡通常看不到。

## 4. ROS 2 Humble：Stage 01 不要求

第一阶段的示例故意不依赖 ROS 2。下面这些能力在[ROS 2 使用基础](15_ros2_basics/README.md)中学习，不再要求直接跳到 Stage 02 的分层调用链；现在全都不会也可以正常开始：

- [ ] 会创建和构建 `ament_cmake` 包；
- [ ] 会编写 publisher/subscription/service/timer；
- [ ] 会用 `ros2 node/topic/service`；
- [ ] 知道 `source /opt/ros/humble/setup.bash` 的作用；
- [ ] 知道当前 `RMW_IMPLEMENTATION`；
- [ ] 能列出 topic type 和 QoS verbose 信息；
- [ ] 知道 callback 是否执行依赖 `spin`，但暂时可以解释不深；
- [ ] 用过 launch 文件。
- [ ] 能区分 action 的目标接受、执行反馈、结果与取消确认。

状态机和行为树的[概念实验](../labs/task_orchestration/README.md)只需 CMake ≥ 3.16 与 C++17 编译器。Lifecycle 官方实验另需 Humble 的 `lifecycle` 包和 `ros2 lifecycle` CLI；BehaviorTree.CPP 框架暂不是入门依赖。准备好哪一层环境就先做对应实验，不能把纯 C++ 通过当作 ROS 集成验证。

推荐先记录环境：

```bash
./tools/check_environment.sh
printenv RMW_IMPLEMENTATION
ros2 doctor --report
```

不要把 `ros2 doctor` 当作万能诊断。它提供环境快照，但不会替你判断通信两端是否匹配、消息是否在排队，或回调为什么尚未执行。

## 5. 目标平台

正式实验基线：

| 项目 | 基线 |
|---|---|
| OS | Ubuntu 22.04 LTS |
| ROS | ROS2 Humble |
| C++ | C++17 |
| Build | ament_cmake + colcon |
| RMW | rmw_fastrtps_cpp 为主，rmw_cyclonedds_cpp 做对照 |
| Tracing | LTTng + ros2_tracing/tracetools |
| Packet analysis | tcpdump + Wireshark |

第一阶段只依赖 Linux/POSIX API，因此旧一点的 Ubuntu 也可以验证机制；正式报告仍以目标平台重新运行，避免把内核、glibc、编译器差异混入结论。

## 6. 环境工具建议

第一次只需确认 `g++`、`cmake` 和 `make` 可用。下面是一套供完整课程使用的工具集合，不必为了开始 Stage 01 一次全部安装：

Ubuntu 22.04 上可安装：

```bash
sudo apt update
sudo apt install \
  build-essential cmake ninja-build \
  strace lsof iproute2 iputils-ping \
  tcpdump wireshark tshark \
  linux-tools-common linux-tools-generic \
  gdb valgrind
```

后续 ROS2 tracing 和 DDS vendor 工具在对应阶段单独安装。Wireshark 抓包权限、`perf_event_paranoid`、network namespace 和 `tc` 会涉及 root/capability；普通 build 与 smoke test 不要求提权。

## 7. 实验记录模板

每次实验复制以下内容到自己的笔记：

```markdown
### 环境
- 日期：
- OS / kernel：
- CPU：
- build type：
- ROS_DISTRO / RMW：

### 猜想
改变 ______ 后，我预计 ______，因为 ______。

### 控制变量
- message size：
- frequency / count：
- transport：
- queue / QoS：

### 证据
- stdout：
- strace：
- ss/lsof：
- pcap/trace：
- CPU/RSS：

### 结果
- 是否支持猜想：
- 反常点：
- 下一次实验：
```

## 8. 留到后面再回答的大问题

不查资料，用两分钟回答：

> 一个 ROS2 进程发送 5 MB PointCloud2 给另一个进程，数据可能在哪里复制、排队、阻塞和被丢弃？

现在只回答出“可能经过发送程序、Linux 或网络、接收程序”就够了。保存第一次答案，完成 Stage 08 后再答一次；两份答案的差异就是这套课程的价值。
