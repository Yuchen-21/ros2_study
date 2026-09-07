# 初学者术语表

这不是需要背诵的单词表，而是阅读课程时的查询页。遇到陌生词就用浏览器搜索或 `Ctrl+F` 查找；先读“白话解释”，等第二遍学习时再关注严格边界。

同一个概念在代码和日志中经常使用英文，所以本文采用“中文（English）”并保留常见缩写。

## 1. 程序、进程与线程

| 术语 | 白话解释 | 容易混淆的地方 |
|---|---|---|
| 程序（program） | 磁盘上的代码和资源，例如一个可执行文件 | 程序还没运行时不是进程 |
| 可执行文件（executable） | 可以被操作系统启动的程序文件 | 本课程也用 executable 指构建出的具体示例程序 |
| 进程（process） | 程序的一次运行实例，有自己的 PID、内存视图和资源 | 同一个程序启动两次会有两个进程 |
| 线程（thread） | 进程中的一条执行路线；同一进程可以有多个线程 | 线程通常共享进程内存，但各自有栈和 TID |
| 父进程 / 子进程（parent / child） | 一个进程创建另一个进程后形成的关系 | “子”不表示它只能做较少工作 |
| PID / TID | Process ID / Thread ID，进程编号 / 线程编号 | 同一进程的多个线程 PID 相同，Linux TID 不同 |
| 节点（node） | ROS 2 中组织 publisher、subscription、timer 等功能的逻辑参与者 | 一个 node 不一定等于一个进程；多个 node 可以在同一进程 |
| 回调（callback） | 某件事发生后由框架调用的函数，例如收到消息后处理它 | 一条 callback 不永久绑定到某一条线程 |
| 守护进程（daemon） | 长时间在后台运行并提供服务的进程 | 它仍然是普通进程，只是运行方式和职责不同 |

## 2. 内存和操作系统边界

| 术语 | 白话解释 | 容易混淆的地方 |
|---|---|---|
| 内核（kernel） | Linux 的核心管理者，负责进程调度、内存、文件、网络和设备 | 它不是某个普通后台应用 |
| 用户态（user space / user mode） | 普通应用代码运行的受限区域 | 应用不能在这里直接控制任意硬件或页表 |
| 内核态（kernel space / kernel mode） | 内核运行并执行受保护操作的区域 | 进入内核态不等于一定换了线程 |
| 虚拟地址（virtual address） | 程序看到的内存地址 | 两个进程打印相同地址，不代表同一物理内存 |
| 地址空间（address space） | 一个进程能看到的整套虚拟地址范围 | 不同进程默认互相隔离 |
| 物理页（physical page） | 实际内存由内核按页管理时使用的单位 | 程序平时使用虚拟地址，不直接选择物理页 |
| 页表（page table） | 把虚拟地址翻译到物理页的映射信息 | 每个进程看到的映射可以不同 |
| MMU | Memory Management Unit，CPU 中协助把虚拟地址翻译为物理地址的硬件单元 | 它依据当前进程的页表工作，所以相同虚拟地址可得到不同结果 |
| 映射（mapping） | 让一段虚拟地址对应某个内存或文件对象 | 两进程可用不同虚拟地址映射同一共享对象 |
| `mmap` | 建立内存映射的系统调用 | `mmap` 本身不自动提供线程安全或所有权规则 |
| Copy-on-Write（COW，写时复制） | `fork` 后父子暂时共享只读物理页，某一方写入时内核再复制 | 暂时共享物理页不表示普通变量的修改会彼此可见 |
| 栈（stack） | 常用于函数局部变量和调用信息的内存区域 | 每个线程有自己的栈 |
| 堆（heap） | 程序动态申请对象时常用的内存区域 | 同一进程的线程共享堆，但仍需并发保护 |
| Page fault（缺页异常） | CPU 访问页面时需要内核补充映射或处理权限的事件 | 不一定是程序崩溃；很多缺页是正常机制的一部分 |

## 3. 数据怎样跨过边界

| 术语 | 白话解释 | 容易混淆的地方 |
|---|---|---|
| IPC | Inter-Process Communication，进程间通信的总称 | pipe、socket、shared memory 都是 IPC 方法 |
| 系统调用（system call / syscall） | 应用请求内核完成受保护操作的入口，例如 `read`、`write`、`send` | 函数调用不一定是系统调用；有些库函数完全在用户态完成 |
| 文件描述符（file descriptor / fd） | 进程用来引用已打开文件、pipe、socket 等内核对象的小整数 | fd 只是进程内的编号，不是对象内容本身 |
| 缓冲区（buffer） | 临时存放待处理数据的一块区域 | 有 buffer 不代表容量无限 |
| 队列（queue） | 按某种次序保存等待处理的项目 | 课程里有多层 queue，必须说明是哪一层 |
| 端点（endpoint） | 通信的一端，例如一个 socket 地址或 DDS writer/reader | “发现端点”不等于已经成功传送用户数据 |
| 阻塞（blocking） | 操作暂时不能完成时，当前线程等待 | 等待可能是正常空闲，也可能是性能问题，要结合上下文判断 |
| 非阻塞（non-blocking） | 操作暂时做不了时立即返回，由程序稍后重试 | 非阻塞不表示操作更快或数据不会丢 |
| 超时（timeout） | 等待超过规定时间后返回失败或走备用路径 | timeout 是避免无限等待的边界，不等于问题根因 |
| EOF | End Of File；对 pipe/TCP 字节流常表示对端已正常关闭且没有更多字节 | EOF 不是一个普通数据字节 |
| `EINTR` | 系统调用被信号打断，程序通常需要按语义重试 | 不一定表示通信本身失败 |
| `EAGAIN` | 当前没有数据或资源暂不可用，常见于非阻塞 I/O | 它通常表示“稍后再试”，具体仍看 API |
| Backpressure（背压） | 接收方跟不上时，压力沿缓冲区向发送方传回 | 有的系统选择阻塞，有的丢旧数据或新数据 |
| 生命周期（lifecycle） | 对象从创建、使用到关闭和清理的全过程 | 忘记关闭一个 fd 可能让对端永远等不到 EOF |

## 4. 字节流、数据报与协议

| 术语 | 白话解释 | 容易混淆的地方 |
|---|---|---|
| 字节流（byte stream） | 一条连续的字节河流，例如 pipe 和 TCP | 发送两次，不保证接收方也正好读取两次 |
| 数据报（datagram） | 一封封有边界的独立消息，例如 UDP datagram | 保留边界不表示一定可靠到达 |
| 消息边界（message boundary） | 一条消息从哪里开始、到哪里结束 | TCP 不替应用保留边界 |
| Framing（分帧） | 在字节流中用固定长度、长度字段或分隔符找出完整消息 | 它不是网络链路层的 Ethernet frame 在本文中的同义词 |
| 协议（protocol） | 通信双方共同遵守的数据格式和行为规则 | 只约定 C++ 类型名通常还不算稳定的跨进程协议 |
| 序列化（serialization） | 把内存中的对象转换成可存储或传输的字节格式 | 反序列化是把字节恢复成可用数据 |
| Wire format（线上格式） | 真正放进通信通道的字节排列 | 它应明确长度、字段、版本和字节序 |
| Native struct | 编译器在当前进程中布局的原生 C/C++ 结构体 | 可能含 padding、指针和 ABI 差异，不宜直接当跨系统协议 |
| ABI | Application Binary Interface，编译后二进制怎样布局和互相调用的约定 | 不同编译器、选项或平台的 ABI 可能不同 |
| Padding（填充） | 编译器为内存对齐插入的空字节 | `sizeof(struct)` 可能大于所有字段大小之和 |
| 字节序（endianness） | 多字节数字的高低位按什么顺序存放 | 网络协议通常明确使用网络字节序，不能靠双方机器碰巧相同 |
| Packet（包） | 网络协议栈某一层传输的数据单位的泛称 | application message、UDP datagram、IP packet 和 Ethernet frame 不是同一层 |
| Fragmentation（分片） | 大数据被拆成更小网络片段传输 | 一个片段丢失可能让整个大样本无法重组 |

## 5. Stage 01 的几种 IPC

| 术语 | 白话解释 | 适用范围或关键特点 |
|---|---|---|
| Pipe（管道） | 由内核托管的一条字节通道 | 常用于有父子关系的本机进程；匿名 pipe 没有路径地址 |
| Socket（套接字） | 应用使用网络或本机通信服务的一类接口和内核对象 | 可以是 TCP、UDP 或 Unix Domain Socket |
| Unix Domain Socket（UDS） | 只在本机使用的 socket | pathname 可让独立进程找到对方，不经过 IP 路由 |
| TCP | Transmission Control Protocol，可靠、有序、双向的字节流协议 | 不保留应用消息边界，丢失时可能等待重传 |
| UDP | User Datagram Protocol，无连接、尽力而为的数据报协议 | 保留 datagram 边界，但不承诺到达、顺序或不重复 |
| IP 地址（IP address） | 在 IP 网络中标识一个网络接口或目的位置的地址 | 它不等于 ROS 2 node 名，也不包含应用端口 |
| 端口（port） | 一台机器上帮助内核把 TCP/UDP 数据交给具体 socket 的编号 | TCP 端口与 UDP 端口属于不同协议空间 |
| Loopback（回环） | 本机访问本机的网络路径，常见地址是 `127.0.0.1` | 仍经过内核网络栈，但不经过物理网卡 |
| NIC（网卡） | Network Interface Controller，把主机接入有线或无线网络的接口/设备 | 回环通信不会经过物理 NIC |
| MTU | Maximum Transmission Unit，一条链路单个网络包可承载大小的重要限制 | 应用消息超过 MTU 时，可能在更高或更低层被拆分 |
| Unicast / Multicast（单播 / 组播） | 单播发给一个目的地；组播发给加入某个组的一批接收者 | 组播不是“保证所有机器都收到”，网络设备和防火墙仍会影响它 |
| Shared Memory（共享内存，SHM） | 让多个进程映射到同一个底层内存对象 | 少搬运大数据，但同步、所有权和崩溃清理需要协议 |
| POSIX | Portable Operating System Interface，一组类 Unix 系统接口标准 | POSIX SHM 是共享内存的一种具体 API，不是所有 SHM 的统称 |

## 6. 并发、同步与所有权

| 术语 | 白话解释 | 容易混淆的地方 |
|---|---|---|
| 并发（concurrency） | 多项工作在重叠时间内推进 | 不一定真的在多个 CPU 核上同时执行；后者常称并行 |
| 同步（synchronization） | 协调多个执行者何时可以读写共享状态 | shared memory 只提供共享，不自动同步 |
| 互斥锁（mutex） | 同一时刻只让一个执行者进入受保护区域 | 锁的范围太大也会造成等待和延迟 |
| 条件变量（condition variable） | 让线程睡眠，等某个受锁保护的条件可能改变后再检查 | 被唤醒不等于条件必然成立，所以通常用 `while` 重查 |
| Semaphore（信号量） | 用计数表示还有多少个资源或项目可用，并支持等待和通知 | 它不自动保证共享数据的所有字段一致 |
| Futex | Fast Userspace Mutex，Linux 提供的一种等待/唤醒机制，pthread 锁在需要睡眠时常借助它 | 看到 `futex` 不一定表示死锁，也可能只是线程正常等待 |
| 原子操作（atomic） | 对其他线程呈现不可分割语义的特定读写操作 | 原子不自动解决一整套多字段协议 |
| Data race（数据竞争） | 多线程无正确同步地访问同一对象，且至少一方写入 | 在 C++ 中这是未定义行为，偶尔输出正确也不代表安全 |
| 竞态条件（race condition） | 结果错误地依赖不可控的执行先后顺序 | 范围比 C++ data race 更广，两者不完全同义 |
| Ownership（所有权） | 谁可以修改、谁负责释放、何时可复用数据 | 能看到一块内存不代表有权随时覆盖它 |
| RAII | Resource Acquisition Is Initialization，用 C++ 对象生命周期自动管理 fd、锁、映射等资源 | 名字抽象，本质是“离开作用域时自动清理” |
| Context switch（上下文切换） | CPU 从执行一个线程切换为执行另一个线程 | 不等于用户态进入内核态的 mode switch |
| Scheduler（调度器） | 操作系统决定哪个可运行线程获得 CPU 的组件 | 数据 ready 不代表目标线程立刻运行 |
| Cache（缓存） | CPU 附近用于加速访问的小而快的存储层 | 多核共享数据仍会产生一致性和带宽成本 |
| Cache coherence（缓存一致性） | 硬件让多个 CPU cache 对共享内存维持一致视图的机制 | 不等于 C++ 程序没有 data race，也不等于没有性能成本 |
| Happens-before | C++ 内存模型中用来保证操作可见顺序的关系 | 日志打印的先后不一定能证明它 |

## 7. ROS 2 通信角色与分层

| 术语 | 白话解释 | 容易混淆的地方 |
|---|---|---|
| ROS 2 | Robot Operating System 2，面向机器人软件的开源框架和工具生态 | 名字中有“操作系统”，但它运行在 Linux 等操作系统之上 |
| Publisher（发布者） | 向某个 topic 发送消息的一端 | `publish()` 返回不表示远端 callback 已执行 |
| Subscription / Subscriber（订阅者） | 声明想接收某个 topic，并在消息可取时处理 | 接收数据和执行 callback 中间仍可能有等待与调度 |
| Topic（话题） | 发布者和订阅者约定使用的命名数据通道 | 它不是一个单独的网络 socket |
| Message（消息） | ROS 2 中有类型的数据样本 | 一个 message 在底层可能被序列化、分片和复制 |
| Service（服务） | 一次请求对应一次响应的通信方式 | 不等同于持续发布的数据流 |
| Action（动作） | 适合耗时任务，可反馈进度并取消 | 通常建立在多种 ROS 通信实体之上 |
| `rclcpp` | ROS 2 的 C++ 客户端库，应用常直接调用这一层 | 它不是 DDS vendor 本身 |
| `rcl` | 位于客户端库之下的 C 语言公共层 | 它通过 RMW 隔离具体中间件实现 |
| RMW | ROS Middleware Interface，ROS 2 对不同中间件实现的统一接口 | `rmw_fastrtps_cpp`、`rmw_cyclonedds_cpp` 是具体实现 |
| Middleware（中间件） | 位于应用与底层传输之间，提供通信抽象和策略的软件 | 不是单指一个网络协议 |
| DDS | Data Distribution Service，数据中心式发布/订阅规范族 | DDS 不只是“ROS 2 网络层”，也包含发现、匹配和 QoS 等语义 |
| RTPS | Real-Time Publish-Subscribe，DDS 常用的网络互操作协议 | DDS 与 RTPS 相关但不是完全同义词 |
| Transport（传输） | 真正搬运数据的下层路径，例如 UDP 或 shared memory | 上层 DDS 可以选择不同 transport |
| Discovery（发现） | 运行中的通信参与者互相找到并交换信息 | 发现 topic 不证明 QoS 匹配或用户数据一定可达 |
| DataWriter / DataReader | DDS 中写数据和读数据的实体 | ROS publisher/subscription 通常映射到它们，但不要假定所有对象都一一对应 |

## 8. 后续章节会见到的词（现在只需能查到）

| 术语 | 白话解释 | 现在需要掌握到什么程度 |
|---|---|---|
| ROSIDL | ROS Interface Definition Language 相关工具链，把 `.msg`、`.srv`、`.action` 或 IDL 定义生成各语言可用代码 | 知道“消息定义会生成代码”即可 |
| Type support（类型支持） | 把 ROS 消息类型接到具体序列化和中间件实现的一组生成代码与接口 | 到 Stage 02 再看调用关系 |
| CDR | Common Data Representation，DDS 常用的二进制数据表示规则 | 知道它是序列化格式即可 |
| DomainParticipant（域参与者） | 一个应用加入某个 DDS 通信域时使用的主要实体 | 不要先假定它和 ROS node 一一对应 |
| GUID | Globally Unique Identifier，分布式系统中标识参与者或实体的编号 | 知道它用于区分“是谁”即可 |
| Locator | RTPS 描述通信地址和传输位置的信息 | 暂时理解为“怎样到达这个端点” |
| SPDP | Simple Participant Discovery Protocol，先发现有哪些 DDS 参与者 | 到 Stage 03 再看抓包字段 |
| SEDP | Simple Endpoint Discovery Protocol，再交换 writer/reader 等端点信息 | 到 Stage 03 再区分它和 SPDP |
| Unicast（单播） | 一个发送方把数据送给一个明确接收地址 | 和 multicast 的接收范围不同 |
| Multicast（组播） | 发送到一个组地址，由加入该组的多个接收者获取 | ROS 2 发现常会使用，但能否跨网络取决于网络配置 |
| NIC | Network Interface Controller，网卡或网络接口设备 | Loopback 不经过物理 NIC |
| MTU | Maximum Transmission Unit，一条链路一次通常能承载的最大包大小 | 数据过大时可能需要分片或在上层拆分 |
| qdisc | Queueing discipline，Linux 网络发送侧的排队和调度规则 | 它不是 DDS history queue |
| Network namespace（网络命名空间） | Linux 为进程提供相互隔离的网卡、路由和端口视图 | 容器可能位于不同 namespace |
| Bridge（桥接器） | 在两套通信系统或网络之间转发并转换数据的组件 | 转发时可能改变队列、类型或可靠性语义 |
| Trace（跟踪） | 在关键事件处记录时间和上下文，再还原执行时间线 | 比普通日志更适合分析跨层时序 |
| LTTng | Linux Trace Toolkit Next Generation，一套低开销跟踪基础设施 | 到 Stage 08 再安装和使用 |
| pcap | Packet capture，保存抓包结果的常见文件格式 | 它记录网络观察点，不包含所有应用内部事件 |
| RSS | Resident Set Size，进程当前驻留在物理内存中的页面规模指标 | 共享页和分配器缓存会影响它，不能直接等同复制量 |
| P50 / P95 / P99 | 50%、95%、99% 的样本不超过该值的分位数 | P99 常用来观察少数较慢样本，不能只看平均值 |

## 9. 调度、QoS 与性能

| 术语 | 白话解释 | 容易混淆的地方 |
|---|---|---|
| Executor（执行器） | ROS 2 中等待可做工作并选择 callback 执行的调度组件 | DDS 收到数据不表示它直接调用普通用户 callback |
| WaitSet（等待集合） | 一次等待多个 ROS 实体“变为可处理”的机制 | ready 表示有工作可取，不表示 callback 已完成 |
| Callback Group（回调组） | 约束哪些 callback 可以并发执行的分组规则 | 使用多线程 Executor 也可能因回调组规则而串行 |
| QoS | Quality of Service，一组关于历史、可靠性、时限等的通信策略 | QoS 不是单一的“质量高低”开关 |
| Reliability（可靠性） | 决定是否尽力补回丢失数据等行为 | 更可靠可能带来重传、排队和更高尾延迟 |
| History / Depth（历史 / 深度） | 决定保留哪些样本以及大致保留多少 | ROS QoS depth 不是 Linux socket queue 的直接长度 |
| Durability（持久性） | 决定后来加入的订阅者能否拿到先前数据等历史语义 | 不等于把数据永久写入磁盘 |
| Latency（延迟） | 一项事件从定义的起点到终点花费的时间 | 必须说明从哪里量到哪里，跨机器还需同步时钟 |
| Jitter（抖动） | 延迟或到达间隔随时间的波动 | 不是单纯“延迟很大” |
| Throughput（吞吐量） | 单位时间处理的数据量 | 高吞吐不保证单条消息低延迟 |
| Tail latency（尾延迟） | 较慢那部分样本的延迟，例如 P99 | 平均值可能掩盖它，而机器人控制常更关心尾部 |
| Freshness（新鲜度） | 收到的数据离产生时刻有多旧 | 消息完整不丢也可能因排队而已经过时 |
| Zero copy（零拷贝） | 在明确的一段路径上省掉某一次或几次数据复制 | 不等于零序列化、零同步、零内存访问，也不保证端到端完全无复制 |
| Loaned message（借用消息） | 中间件把缓冲区暂时借给应用填写或读取，之后归还 | 是否支持取决于消息类型、RMW 和具体实现 |
| Intra-process communication | 同一进程内的通信优化 | 不等于两个进程之间的 shared-memory transport |

## 10. 常见观察工具

| 工具 | 一句话用途 | 第一次是否必学 |
|---|---|---:|
| `ps` / `top` | 看进程、线程和 CPU 状态 | 认识即可 |
| `/proc` | Linux 暴露进程、内存等运行信息的虚拟文件系统 | 选学 |
| `strace` | 看程序调用了哪些系统调用，以及在哪里等待 | 第二遍再学 |
| `ss` | 看 TCP、UDP、UDS 端点和部分队列状态 | 第二遍再学 |
| `lsof` | 看一个进程打开了哪些文件、socket 或共享对象 | 第二遍再学 |
| `tcpdump` / Wireshark | 抓取并分析经过指定网络接口的数据包 | 学 TCP/UDP 时再学 |
| `perf` | 观察 CPU 事件、热点和调度等性能信息 | 深挖时再学 |
| GDB | GNU Debugger，可暂停程序、查看变量和调用栈 | 源码阶段再学 |

## 11. 看到一个新词时怎么处理

建议按这个顺序：

1. 先问“它是谁或是什么东西？”；
2. 再问“它在数据路径上做什么？”；
3. 最后才问“它内部怎样实现？”。

例如第一次看到 Executor，知道“它负责选择并运行 ROS 2 callback”就足够继续。等学到调度章节，再补 WaitSet、线程池和 Callback Group。不要让第三层细节挡住第一层主线。
