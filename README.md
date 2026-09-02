# ROS2 Humble 进程间通信与机器人中间件系统教程

这不是一本 `rclcpp` API 速查手册，而是一套面向机器人系统软件/中间件工程师的实验课程。主线问题只有两个：

1. `publisher_->publish(msg)` 之后，数据究竟经过了哪些用户态对象、队列、线程、内核缓冲区和网卡？
2. 数据到达以后，为什么是某个 Executor 线程在某个时刻调用 callback；延迟、抖动和丢包又产生在哪里？

课程固定采用：

> 理论 → 生动类比 → Linux/C++ 实验 → ROS2 实验 → 源码观察 → 性能实验 → 面试表达

目标平台是 Ubuntu 22.04、ROS2 Humble、C++17。第一阶段的 Linux IPC 包也支持脱离 ROS2 的普通 CMake 构建，便于先把底层机制看清。

## 当前交付状态

| 内容 | 状态 | 说明 |
|---|---:|---|
| 完整课程目录与 14 阶段 Roadmap | ✅ | 每阶段均给出目标、实验、产出和求职映射 |
| 前置知识自检 | ✅ | 不是考试，用来决定哪些基础需补齐 |
| 通信“大图”v0.1 | ✅ | 已标注队列、线程、复制、内核边界和延迟点 |
| 第一阶段 Linux IPC 教程 | ✅ | Process/Thread、pipe、UDS、TCP/UDP、shared memory |
| 第一阶段 C++17 实验 | ✅ | 12 个 executable、CTest 与端到端 smoke test |
| Ubuntu 22.04 / Humble 验证 | ✅ | [构建、测试矩阵与非保证项](docs/verification.md) |
| 第二至十四阶段正文与实现 | 🧭 已规划 | 按课程顺序逐阶段实现 |

## 课程目录

```text
ros2_communication_lab/
├── README.md
├── docs/
│   ├── 00_course_map.md
│   ├── 00_prerequisites.md
│   ├── ros2_communication_big_picture.md
│   ├── 01_linux_ipc/
│   ├── 02_ros2_architecture/
│   ├── 03_dds_discovery/
│   ├── 04_executor_waitset/
│   ├── 05_qos/
│   ├── 06_serialization_zero_copy/
│   ├── 07_shared_memory_transport/
│   ├── 08_tracing_performance/
│   ├── 09_network_multihost/
│   ├── 10_troubleshooting/
│   ├── 11_middleware_comparison/
│   ├── 12_source_dive/
│   ├── 13_interview/
│   └── 14_capstone/
├── labs/
│   └── 01_linux_ipc/linux_ipc_lab/
├── src/                 # 后续阶段复用的 ROS2 packages
├── scripts/
├── tools/
├── benchmarks/
├── interview/
└── capstone/
```

## 推荐学习路线

先完成[前置知识自检](docs/00_prerequisites.md)，再打开[完整课程地图](docs/00_course_map.md)。课程顺序不是按 ROS2 API 分类，而是按一条消息的真实生命周期组织：

```text
进程/线程与 Linux IPC
        ↓
ROS2 分层与 publish 调用链
        ↓
DDS/RTPS Discovery 与传输
        ↓
Executor/WaitSet/Callback 调度
        ↓
QoS、队列与背压
        ↓
序列化、复制与 Zero Copy
        ↓
Tracing、网络实验与系统化排障
        ↓
架构比较、源码阅读、面试与综合项目
```

完整知识关系持续维护在[ROS2 Communication Big Picture](docs/ros2_communication_big_picture.md)。

## 14 阶段导航

| 阶段 | 章节 | 当前状态 |
|---:|---|---:|
| 01 | [Linux IPC：进程、pipe、socket、shared memory](docs/01_linux_ipc/README.md) | 完整教程 + 可运行实验 |
| 02 | [ROS2 分层与 publish 生命周期](docs/02_ros2_architecture/README.md) | 规划 |
| 03 | [DDS、RTPS 与 Discovery](docs/03_dds_discovery/README.md) | 规划 |
| 04 | [Executor、WaitSet 与 CallbackGroup](docs/04_executor_waitset/README.md) | 规划 |
| 05 | [DDS QoS、History 与背压](docs/05_qos/README.md) | 规划 |
| 06 | [Serialization、Intra-process 与 Zero Copy](docs/06_serialization_zero_copy/README.md) | 规划 |
| 07 | [ROS2/DDS Shared Memory Transport](docs/07_shared_memory_transport/README.md) | 规划 |
| 08 | [Tracing、Performance 与 Profiler](docs/08_tracing_performance/README.md) | 规划 |
| 09 | [网络、多机、Docker 与 namespace](docs/09_network_multihost/README.md) | 规划 |
| 10 | [Communication Troubleshooting Playbook](docs/10_troubleshooting/README.md) | 规划 |
| 11 | [DDS 与 CyberRT/LCM/MQTT/Zenoh/ZeroMQ](docs/11_middleware_comparison/README.md) | 规划 |
| 12 | [ROS2 Humble 源码阅读路线](docs/12_source_dive/README.md) | 规划 |
| 13 | [ROS2 中间件工程师 100 问](docs/13_interview/README.md) | 框架 + Stage 01 题库 |
| 14 | [mini_robot_middleware_lab](docs/14_capstone/README.md) | 规划 |

## 第一阶段：立即开始

阅读顺序：

1. [阶段导读与学习契约](docs/01_linux_ipc/README.md)
2. [Process、Thread、虚拟内存与上下文切换](docs/01_linux_ipc/01_process_thread_memory.md)
3. [匿名管道：继承文件描述符的字节流](docs/01_linux_ipc/02_pipe.md)
4. [Unix Domain Socket：本机有地址的 IPC](docs/01_linux_ipc/03_unix_domain_socket.md)
5. [TCP、UDP 与机器人数据流](docs/01_linux_ipc/04_tcp_udp.md)
6. [POSIX Shared Memory：共享页、同步与所有权](docs/01_linux_ipc/05_shared_memory.md)
7. [工具观察与 IPC 选型](docs/01_linux_ipc/06_observation_and_comparison.md)
8. [逐条实验手册](docs/01_linux_ipc/07_lab_manual.md)

### 普通 CMake 构建

这条路径只需要 Linux、CMake、G++ 和 pthread：

```bash
./scripts/build_stage1.sh
./scripts/run_stage1_smoke.sh
```

脚本执行的原因：

- `build_stage1.sh` 强制 C++17、导出 `compile_commands.json`，并执行编译器警告检查；
- `run_stage1_smoke.sh` 会依次验证进程隔离、pipe 的 EOF、UDS、TCP、UDP 消息边界和共享内存同步，任何子实验异常都会立即失败；
- 所有临时 socket、共享内存名称和日志均限制在实验命名空间内。

手工构建等价命令：

```bash
cmake -S labs/01_linux_ipc/linux_ipc_lab \
      -B build/stage1 \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/stage1 --parallel
ctest --test-dir build/stage1 --output-on-failure
```

如果同一源码目录通过不同绝对路径挂载到容器，不要复用宿主机 CMake cache；指定独立构建树：

```bash
STAGE1_BUILD_DIR=/tmp/ros2_comm_lab_stage1_build +  ./scripts/build_stage1.sh
STAGE1_BUILD_DIR=/tmp/ros2_comm_lab_stage1_build +  ./scripts/run_stage1_smoke.sh
```

### ROS2 Humble / colcon 构建

```bash
source /opt/ros/humble/setup.bash
colcon build \
  --base-paths labs/01_linux_ipc \
  --packages-select linux_ipc_lab \
  --symlink-install \
  --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo
colcon test --packages-select linux_ipc_lab
colcon test-result --verbose
source install/setup.bash
ros2 run linux_ipc_lab thread_shared_state
```

同一份 `CMakeLists.txt` 会在找到 `ament_cmake` 时注册 ROS2 包；未找到时保持普通 CMake 可构建。第一阶段的程序本身故意不依赖 `rclcpp`，否则很难分辨现象来自 Linux 还是 ROS2。

### 推荐观察工具

```bash
./tools/check_environment.sh
strace -ff -ttT -e trace=process,network,read,write \
  build/stage1/pipe_process_a
ss -xap
ss -ltnup
lsof -U
```

`netstat` 可以使用，但 `ss` 直接读取内核 socket 信息，通常更快、字段也更适合现代 Linux。`tcpdump`/Wireshark 能看到 TCP、UDP 和后续 RTPS 流量，却看不到匿名 pipe，也看不到 UDS 数据包经过物理网卡——这正是实验要建立的工具边界感。

## 完成本轮后应能做到

- [ ] 我能解释 thread 共享地址空间，而 process 默认只有相互隔离的虚拟地址空间。
- [ ] 我能指出一次 pipe/UDS/TCP/UDP 发送中的用户态与内核态边界。
- [ ] 我能解释 TCP 是字节流、UDP/UDS datagram 保留消息边界。
- [ ] 我能解释 shared memory 快在哪里，以及为什么它仍需要同步、所有权和缓存一致性。
- [ ] 我能使用 `ps`、`/proc`、`strace`、`ss`、`lsof`、`tcpdump` 为 IPC 机制选择正确观察点。
- [ ] 我能从复制次数、系统调用、调度、队列和协议语义，而不是只从“快/慢”比较 IPC。
- [ ] 我能在面试白板上把 Linux IPC 与后续 DDS transport、shared-memory transport 联系起来。

## 工程约定

- C++17，RAII 管理 fd、mmap 和 socket path；
- 网络实验显式序列化为网络字节序，不直接发送有 padding 的 C++ struct；
- 所有接收端输出 sequence、payload、单调时钟单向延迟；
- 使用固定回环地址和实验专用端口，默认不暴露到外部网络；
- 代码注释解释“为什么”而不是复述语句；
- 文档对 Humble 和新版本行为分开标注；
- 基准结果不写死：硬件、内核、RMW 和 DDS 配置必须与数字一起记录。

## 许可证

Apache-2.0。课程内容、代码与实验可以用于个人学习和作品集；引用第三方项目时遵循其各自许可证。
