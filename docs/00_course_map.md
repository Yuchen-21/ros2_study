# 完整课程 Roadmap

## 1. 怎么使用这张地图

这套课程按“依赖关系”而不是按 API 名称排序。建议每周 8～12 小时，完整走完约 14～20 周；有 ROS1/ROS2 项目经验时，可以快速阅读熟悉部分，但不要跳过实验猜想和观测记录。

重要度标记：

- ⭐：必须掌握，能解释基本机制；
- ⭐⭐：重要，能完成实验并分析现象；
- ⭐⭐⭐：面试高频 / 工程核心，要求能在白板上建立因果链并排障。

每阶段通过四类 Gate：

1. **Explain**：不用术语堆砌，能讲清“谁拥有数据、谁在等待、谁唤醒谁”；
2. **Observe**：能用 Linux/ROS2 工具看到机制留下的证据；
3. **Modify**：能改变队列、线程、QoS 或网络条件，预测结果；
4. **Diagnose**：面对一个故障，从 Application 一直查到 OS/Network。

## 2. 一条消息贯穿全课程

```text
Application message
  │ publish()
  ▼
rclcpp typed publisher
  │ rcl_publish()
  ▼
rcl (C client abstraction)
  │ rmw_publish()
  ▼
RMW implementation
  │ DDS DataWriter / type support
  ▼
DDS history cache ── CDR serialization ── RTPS writer
  │
  ├── UDP/IP → kernel socket buffers → NIC → network
  └── shared-memory transport → shared segment + synchronization
                                                    │
                                                    ▼
RTPS reader → DDS DataReader cache → rmw/rcl readiness
                                                    │
                                              WaitSet wakes
                                                    │
                                            Executor selects work
                                                    │
                                           callback-group admission
                                                    │
                                            user callback executes
```

所有阶段都在回答这张图的一部分，最终在 `ros2_communication_big_picture.md` 合并。

## 3. 阶段总览

| 阶段 | 核心问题 | 关键实验 | 求职能力 | 重要度 |
|---:|---|---|---|---:|
| 01 Linux IPC | 两个进程为何不能直接读彼此变量？数据如何跨边界？ | thread/process、pipe、UDS、TCP、UDP、POSIX shm | Linux 系统编程、传输选型、底层排障 | ⭐⭐⭐ |
| 02 ROS2 分层 | `publish()` 穿过了哪些稳定抽象层？ | 双 RMW 运行、调用栈/符号、typed/serialized publish | ROS2 中间件原理、接口边界 | ⭐⭐⭐ |
| 03 DDS/RTPS | 没有 ROS Master，端点如何发现与匹配？ | SPDP/SEDP 抓包、Domain ID、GUID、发现时序 | DDS 网络排障、多机部署 | ⭐⭐⭐ |
| 04 Executor | 数据已到，callback 为什么此时才运行？ | 1/多线程 executor、callback group、阻塞注入 | 并发设计、抖动/饥饿/死锁定位 | ⭐⭐⭐ |
| 05 QoS | “可靠”为什么可能让实时性更差？ | 丢包/时延/慢订阅者、QoS 不兼容矩阵 | 传感器/控制 QoS 设计 | ⭐⭐⭐ |
| 06 序列化与 Zero Copy | CPU 花在何处，所谓零拷贝到底省了哪次复制？ | 1 KB～10 MB、IPC/intra-process/loaned message | 大消息优化、准确术语边界 | ⭐⭐⭐ |
| 07 DDS Shared Memory | 同机大数据如何绕开 UDP 栈？ | Fast DDS UDP-only vs SHM、5 MB benchmark | 相机/点云吞吐优化 | ⭐⭐ |
| 08 Tracing/Performance | 延迟是发送、网络、排队还是 callback 调度造成的？ | LTTng、tracetools、perf、通信 profiler | 性能建模与证据链 | ⭐⭐⭐ |
| 09 多机网络 | 为什么 topic 可见却收不到样本？ | netns/Docker、multicast、MTU、tc、firewall | 现场网络部署与故障定位 | ⭐⭐⭐ |
| 10 排障 Playbook | 如何避免一上来就“重启 DDS”？ | 10 个真实故障、逐层检查、自动采证 | 系统化 incident response | ⭐⭐⭐ |
| 11 中间件比较 | DDS、CyberRT、LCM、MQTT、Zenoh 各在解决什么？ | 同负载小型适配器/架构决策记录 | 架构选型与 bridge 设计 | ⭐⭐⭐ |
| 12 源码阅读 | 如何追调用链而不迷失在数百万行代码？ | `publish()` 与 `spin()` 两条 Humble 路线 | 源码定位、二次开发 | ⭐⭐ |
| 13 面试专题 | 如何用 30 秒、2 分钟和深挖三层表达？ | 100 问、白板、故障推演 | 中间件岗位表达与深度 | ⭐⭐⭐ |
| 14 Capstone | 能否把机制整合成可运行机器人通信系统？ | mini_robot_middleware_lab + 故障注入 | GitHub 作品、系统设计闭环 | ⭐⭐⭐ |

## 4. 各阶段学习目标、实验和求职映射

### Stage 01 — Linux IPC 基础

**学习目标**

- 建立 process、thread、虚拟地址空间、用户态/内核态、调度与上下文切换的精确模型；
- 区分字节流、数据报、共享页三类通信语义；
- 从复制、系统调用、同步、背压、消息边界和故障域比较 IPC；
- 理解 shared memory 不是自动同步，也不等价于 zero copy。

**实验清单**

- 线程共享同一对象地址并安全累加；
- `fork()` 后相同虚拟地址、不同物理语义（copy-on-write）；
- `pipe2() → fork() → exec()` 的匿名管道；
- AF_UNIX/SOCK_DGRAM publisher/subscriber；
- TCP framing 与 UDP datagram；
- `shm_open + ftruncate + mmap`，配合 process-shared mutex/condition variable；
- 用 `strace/ss/lsof/tcpdump/proc` 区分各机制。

**【求职价值】** Linux IPC 是理解 DDS transport 和共享内存的地基。面试官真正关心的是你能否分析复制路径、阻塞、EOF、partial read、同步和生命周期，而不是能否背出 IPC 名称。

**阶段产出**：可独立构建的 `linux_ipc_lab` 包、smoke test、实验报告模板和 Stage 1 面试题。

### Stage 02 — ROS2 通信分层与 publish 生命周期

**学习目标**

- 准确界定 application、`rclcpp`、`rcl`、`rmw`、DDS、RTPS、transport；
- 追踪 Humble 的 typed publish 路径：`rclcpp::Publisher<T>::publish` → `do_inter_process_publish` → `rcl_publish` → `rmw_publish` → vendor DataWriter；
- 解释 ROSIDL typesupport 与 RMW 实现是如何在运行期接合的；
- 识别 API 层、策略层和 vendor 实现层，避免把 DDS 说成“网络层”。

**实验清单**

- `RMW_IMPLEMENTATION` 切换 Fast DDS/Cyclone DDS并比较进程加载库；
- `LD_DEBUG=libs`、`ldd`、`/proc/PID/maps` 观察动态链接；
- GDB breakpoint 在 `rcl_publish/rmw_publish`；
- typed message 与 `rclcpp::SerializedMessage` 路径对比；
- 用最小 publisher 标记应用时间戳。

**【求职价值】** 能解释抽象边界，才能做 RMW 适配、vendor 切换、源码定位和性能归因。

### Stage 03 — DDS、RTPS 与分布式 Discovery

**学习目标**

- 映射 ROS Node/Publisher/Subscription/Topic 到 DDS 实体，并明确它不是简单一一对应：ROS publisher/subscription 通常落实为 DataWriter/DataReader，而 Node 与 DomainParticipant 的复用关系由 RMW 实现/context 决定；
- 解释 RTPS participant、GUID/EntityId、locator、builtin endpoints；
- 分清 SPDP participant discovery 与 SEDP endpoint discovery；
- 理解 Domain ID、type name、topic name、QoS compatibility 共同决定匹配。

**实验清单**

- talker/listener 启动前后抓取 RTPS；
- Wireshark `rtps` 过滤 SPDP、SEDP、DATA、HEARTBEAT、ACKNACK；
- `ROS_DOMAIN_ID=10/20` 隔离；
- 延迟启动 listener，绘制 discovery/match 时间线；
- 禁用 multicast 或施加 firewall，比较发现与数据路径；
- Fast DDS discovery server 作为扩展，不把它误认为 ROS Master。

**【求职价值】** “topic list 看得到但 echo 没数据”经常位于发现成功、endpoint 未匹配或数据 locator 不通之间。理解两阶段发现是现场排障核心。

### Stage 04 — Executor、WaitSet 与 Callback

**学习目标**

- 建立 entity readiness → WaitSet → Executor selection → CallbackGroup admission → user callback 的链路；
- 回答 DDS 接收线程是否直接执行普通 ROS callback；
- 比较 `spin()`、`spin_once()`、`spin_some()` 和三种 Humble executor；
- 理解 mutually-exclusive、reentrant、线程池、饥饿、优先级反转与死锁。

**实验清单**

- 两个 subscription + timer 打印 timestamp/thread id；
- SingleThreaded vs MultiThreaded；
- 500 ms 慢 callback 与 callback-group 组合矩阵；
- service 内同步等待同组 future，重现 executor deadlock；
- WaitSet 手动调度；
- tracetools 测 callback ready 到 start 的 scheduling delay。

**【求职价值】** 控制、传感器、timer 和网络回调并存时，线程模型错误会造成控制周期抖动、callback starvation、deadlock 和 priority inversion。这是“会用 ROS2”和“能设计机器人运行时”的分界。

### Stage 05 — QoS 是分布式资源与时序策略

**学习目标**

- 从 DDS history cache、匹配兼容、重传与 resource limits 解释 QoS；
- 理解 reliability、durability、history/depth、deadline、lifespan、liveliness 的独立职责；
- 解释 RELIABLE 为何可能增加排队、重传、内存和尾延迟。

**实验清单**

- 1000 Hz publisher + 慢 subscriber；
- depth 1/10/100 的丢样本与 age 分布；
- BEST_EFFORT/RELIABLE 在 `tc netem` loss/delay/jitter 下比较；
- VOLATILE/TRANSIENT_LOCAL 晚加入；
- QoS incompatible event；
- deadline/liveliness 事件；
- camera/lidar/control/map/config QoS 决策表。

**【求职价值】** 真实机器人不会用一份默认 QoS 覆盖所有 topic。能把业务 freshness/可靠性需求翻译成 DDS 行为，是中间件岗位高频能力。

### Stage 06 — ROSIDL、CDR、复制与 Zero Copy

**学习目标**

- 从 IDL 到生成类型支持，再到 CDR serialized sample；
- 标出 application copy、serialization copy、DDS cache copy、kernel copy；
- 严格区分 intra-process、inter-process、shared memory transport、loaned message、zero copy；
- 了解 RMW 和消息类型约束使 loaning 能力不同。

**实验清单**

- 自定义 variable-size payload，1 KB/100 KB/1 MB/10 MB；
- latency、CPU、吞吐、allocation 次数曲线；
- intra-process on/off；
- serialized publish；
- `borrow_loaned_message` 支持探测和失败降级；
- 固定容量/动态容量消息对 loaning 的影响。

**【求职价值】** Image/PointCloud2 吞吐优化不能靠一句“开零拷贝”。需要知道究竟省了哪次复制、谁拥有 buffer、何时归还。

### Stage 07 — Fast DDS Shared Memory Transport

**学习目标**

- 理解 DDS shared-memory transport 是 locator/segment/port/notification 机制，而非 ROS intra-process；
- 掌握 Humble 所带 Fast DDS 版本与上游新版本配置差异；
- 比较 UDP-only、SHM-enabled、同进程 IPC 的边界。

**实验清单**

- 两进程 5 MB sample，强制 UDP-only 与 SHM-only/优先 SHM；
- `/dev/shm`、进程 maps、Fast DDS 日志观察；
- perf、topic bw/hz、RSS、tail latency；
- publisher 崩溃、segment 清理与端口冲突；
- PointCloud/Image 数据速率预算。

**【求职价值】** 多相机/多雷达系统的瓶颈经常是内存带宽和复制，而不仅是 NIC 带宽。

### Stage 08 — Tracing 与性能工程

**学习目标**

- 定义并区分 transport latency、callback latency、scheduling delay、queueing delay、callback duration、jitter；
- 用 tracepoint 建立因果链，而不是依赖 printf；
- 理解 `ros2 topic hz/bw` 的观察者效应和口径限制。

**实验清单**

- LTTng + `ros2_tracing` 记录 publish、`rmw_publish`、callback start/end；
- 消息内时间戳与 trace 时间关联；
- perf stat/record、火焰图；
- `/proc/PID/stat`、`status`、`sched`；
- Communication Profiler：Hz、size、inter-arrival、age、callback duration、CPU/RSS；
- P50/P95/P99/max 和直方图。

**【求职价值】** 性能岗位需要用时间线证明瓶颈，能把“ROS2 很慢”拆成可测量的阶段。

### Stage 09 — 网络与多机 ROS2

**学习目标**

- 理解 multicast discovery 与 unicast user data 并非永远固定；
- 解释网卡选择、route、MTU/fragmentation、socket buffer、Docker NAT/bridge/host；
- 能区分发现平面和数据平面故障。

**实验清单**

- 两个 network namespace 或容器模拟 Robot/Sensor Computer；
- veth + bridge + tcpdump 双端抓包；
- `tc netem` loss/delay/jitter/rate；
- nftables/iptables 阻断 multicast 或用户数据端口；
- MTU 不一致与大样本碎片；
- Fast DDS/Cyclone DDS interface allowlist 配置。

**【求职价值】** 多机部署失败往往不是 C++ bug，而是 multicast、路由、防火墙、容器网络或 MTU。中间件工程师必须能跨边界定位。

### Stage 10 — Communication Troubleshooting Playbook

**学习目标**

- 固化 Application → Executor → RMW → DDS → Transport → Network → OS 的逐层排查；
- 先收集证据，再变更配置；
- 建立可复用 incident bundle。

**实验清单**

- publisher 调用但 subscriber 不 callback；
- topic list 可见但无样本；
- 100 Hz 变 40 Hz；
- CPU 100%、callback hang、Wi-Fi 不稳；
- Domain ID/QoS mismatch；
- PointCloud 拖垮系统；
- 单线程 executor 被慢 callback 阻塞；
- `collect_ros2_comm_diagnostics.sh` 自动采集。

**【求职价值】** 面试 case 和现场 incident 都看“缩小故障域的顺序”，不是命令数量。

### Stage 11 — 与 CyberRT、LCM、MQTT、Zenoh、ZeroMQ 比较

**学习目标**

- 从 discovery、transport、serialization、QoS、routing、shared memory、实时性和运维比较设计取舍；
- 区分 library、brokered messaging、data-centric pub/sub、router-based fabric；
- 设计 DDS + SHM + bridge + WebSocket 的混合系统。

**实验清单**

- 对同一 SensorData 定义适配器接口；
- 延迟/吞吐/故障语义对比，避免不公平 benchmark；
- LiDAR/Camera/INS/Planning/Control/CAN/Foxglove 通信决策；
- 写 Architecture Decision Record；
- bridge 背压与时间戳保持。

**【求职价值】** 高级岗位需要说明“为什么选择”，包括不选某技术的约束，而不是背产品功能表。

### Stage 12 — ROS2 Humble 源码阅读路线

**学习目标**

- 以问题和断点驱动源码阅读；
- 掌握 demo_nodes_cpp → rclcpp → rcl → rmw → rmw_fastrtps_cpp → Fast DDS 的边界；
- 分别追 `publish()` 与 `Executor::spin()`。

**实验清单**

- 建立 Humble 源码工作区和 compile database；
- rg/ctags/clangd 查虚函数与模板实例；
- GDB 条件断点和 shared-library breakpoint；
- 画调用路径并记录“此层拥有的策略”；
- 切换 rmw 验证边界。

**【求职价值】** 源码能力体现在能快速定位正确仓库和抽象层，而不是阅读行数。

### Stage 13 — 中间件工程师 100 问

**学习目标**

- 为每题形成 30 秒结论、2 分钟因果链、深入追问证据；
- 遇到版本/RMW 差异时主动声明前提；
- 系统设计题能给出容量、线程、QoS、故障和观测方案。

**实验清单**

- 基础/中级/高级/系统设计四组 25 题；
- 白板还原端到端链路；
- 故障现场口述；
- 录音复盘术语和逻辑；
- mock interview 评分表。

**【求职价值】** 把工程经验压缩成结构化表达，避免答案只有名词或只有项目故事。

### Stage 14 — mini_robot_middleware_lab

**学习目标**

- 将 QoS、executor、callback group、tracing、SHM、网络模拟和监控整合；
- 建立容量预算、SLO、故障注入和复盘闭环；
- 形成可展示、可复现、可解释的 GitHub 项目。

**实验清单**

- lidar 10 Hz/camera 30 Hz 大消息；
- localization 100 Hz/planning 20 Hz/control 100 Hz；
- monitor 统计 Hz、bandwidth、age、callback duration、CPU/RSS；
- Foxglove bridge 与 MCAP 记录；
- packet loss、slow callback、QoS mismatch、CPU overload、network delay；
- UDP/SHM 与 executor 配置对照；
- 自动生成实验报告。

**【求职价值】** Capstone 用代码、trace、抓包和架构决策共同证明系统能力，不只是一个节点集合。

## 5. 里程碑与验收

| 里程碑 | 白板能力 | 实操能力 | 证据 |
|---|---|---|---|
| M1：IPC | 画出每种 IPC 的边界与复制 | 编译并 strace 第一阶段 | smoke test + 实验记录 |
| M2：ROS2/DDS | 讲清 publish/discovery | 抓 SPDP/SEDP 与切换 RMW | pcap + 调用链图 |
| M3：调度/QoS | 解释 callback 与队列 | 重现阻塞、丢包、QoS 不兼容 | trace + netem 报告 |
| M4：性能 | 标出端到端 latency budget | profiler + perf + SHM 对照 | P99 报告 |
| M5：系统 | 做中间件/网络/线程决策 | Capstone 故障注入与定位 | ADR + incident report |
| M6：面试 | 三档回答 100 问 | 45 分钟 mock system design | 评分表 |

## 6. 全课程完成 Checklist

- [ ] 我能解释 Process 与 Thread 的共享与隔离边界。
- [ ] 我能解释 `publisher_->publish()` 的 Humble 调用链。
- [ ] 我能解释 SPDP、SEDP、GUID、locator 和 endpoint matching。
- [ ] 我能解释 Executor、WaitSet、CallbackGroup 与 DDS 线程的关系。
- [ ] 我能用 history cache 和网络状态解释 QoS 行为。
- [ ] 我能区分 serialization、copy、intra-process、loaning、SHM 和 zero copy。
- [ ] 我能抓取并阅读基本 RTPS 流量。
- [ ] 我能用 tracing 把 latency 分段。
- [ ] 我能定位 topic 可见但收不到、频率下降、CPU 打满和 callback 卡顿。
- [ ] 我能为 LiDAR/Camera/Control/Visualization 选择不同通信路径。
- [ ] 我能比较 DDS、CyberRT、LCM、MQTT、Zenoh、ZeroMQ 的设计边界。
- [ ] 我能按问题追踪 Humble 源码而不迷失。
- [ ] 我能完成 100 问的三层表达。
- [ ] 我能演示并解释 mini_robot_middleware_lab 的故障注入结果。
