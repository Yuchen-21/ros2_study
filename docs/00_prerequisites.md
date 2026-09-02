# 前置知识与环境自检

这不是准入考试。勾不上的项目决定你在哪些地方放慢速度；它们不应阻止你开始 Stage 01。

## 1. C++17 自检

### 必须能读懂

- [ ] 值、引用、指针、`const` 的区别；
- [ ] 栈对象析构与 RAII；
- [ ] `std::unique_ptr`/`std::shared_ptr` 的所有权含义；
- [ ] `std::thread`、`std::mutex`、`std::condition_variable`；
- [ ] `std::chrono::steady_clock`；
- [ ] 异常与返回码的边界；
- [ ] 模板类的基本阅读能力。

### 建议补齐

- [ ] move semantics；
- [ ] `std::atomic` 和 memory order 的目的；
- [ ] alignment、padding、trivially copyable；
- [ ] 虚函数、type erasure 和 shared library symbol；
- [ ] CMake target-based 写法。

**快速检查题**：为什么不能把含有 `std::string` 的 C++ struct 直接 `send(fd, &object, sizeof(object), ...)` 给另一个进程？

合格回答应包含：对象内是指针/容量等进程内状态；虚拟地址对另一个进程无意义；ABI、padding、字节序与版本也不稳定；需要定义 wire format 和序列化。

## 2. Linux 系统能力自检

- [ ] 能解释 PID、TID、文件描述符；
- [ ] 知道系统调用为何从用户态进入内核态；
- [ ] 会读 `ps -eLf`、`top -H`、`/proc/PID/status`；
- [ ] 理解 blocking、non-blocking、timeout；
- [ ] 知道 `SIGINT` 与进程退出的关系；
- [ ] 会使用 `ip addr`、`ip route`、`ss`；
- [ ] 知道端口、IP 地址、loopback 和 NIC 的区别；
- [ ] 能用 `strace` 观察系统调用；
- [ ] 知道 root/capability 为什么影响 `tcpdump`、`tc`、network namespace。

**快速检查题**：`write(fd, buffer, 1000)` 返回 400 是否一定是错误？

不是。字节流/非阻塞 I/O 可能发生 partial write；程序要按已写字节推进。实验的 `write_all` 正是为这个语义存在。对 datagram，partial datagram 的处理语义不同，不能套用字节流循环。

## 3. 网络基础自检

- [ ] 分清 Ethernet frame、IP packet、UDP datagram、TCP byte stream；
- [ ] 理解 MTU 与 IP fragmentation 的基本关系；
- [ ] 理解 TCP reliability 不等于应用实时性；
- [ ] 知道 multicast 与 broadcast/unicast 的区别；
- [ ] 知道 socket send buffer 和 receive buffer 会排队；
- [ ] 能解释 packet loss、latency、jitter、reordering；
- [ ] 对抓包过滤表达式有基本认识。

**快速检查题**：为什么同机的 `127.0.0.1` UDP 也不是“直接函数调用”？

它仍经过 socket API、内核网络栈、socket buffer 和调度；只是不会从物理 NIC 发出。抓 `lo` 能看到，抓物理网卡通常看不到。

## 4. ROS2 Humble 自检

- [ ] 会创建和构建 `ament_cmake` 包；
- [ ] 会编写 publisher/subscription/service/timer；
- [ ] 会用 `ros2 node/topic/service`；
- [ ] 知道 `source /opt/ros/humble/setup.bash` 的作用；
- [ ] 知道当前 `RMW_IMPLEMENTATION`；
- [ ] 能列出 topic type 和 QoS verbose 信息；
- [ ] 知道 callback 是否执行依赖 `spin`，但暂时可以解释不深；
- [ ] 用过 launch 文件。

推荐先记录环境：

```bash
./tools/check_environment.sh
printenv RMW_IMPLEMENTATION
ros2 doctor --report
```

不要把 `ros2 doctor` 当作万能诊断。它提供环境快照，不会替你判断 endpoint matching、队列 age 或 callback scheduling。

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

## 8. 开始前的口头基线

不查资料，用两分钟回答：

> 一个 ROS2 进程发送 5 MB PointCloud2 给另一个进程，数据可能在哪里复制、排队、阻塞和被丢弃？

现在答不完整是正常的。保存第一次答案，完成 Stage 08 后再答一次；两份答案的差异就是这套课程的价值。
