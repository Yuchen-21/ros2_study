# Stage 01 面试专题：Linux IPC 三层回答

使用方法：先只读题并录下 30 秒首答；再对照。30 秒版本给结论，2 分钟版本展开因果链，深入追问用于验证是否真正做过实验。

## Q1. Thread communication 与 process communication 的根本区别是什么？

### 30 秒回答

同一进程的线程默认共享一个虚拟地址空间，所以能通过对象、指针和共享变量通信，但必须同步并发访问。不同进程默认有隔离的虚拟地址空间，A 的指针对 B 没有对象语义；必须通过 pipe/socket 等内核通道复制字节，或显式 `mmap` 同一个 shared-memory backing。两种方式在性能、故障隔离和并发风险上不同。

### 2 分钟回答

Thread 是内核调度实体，但同一进程的线程共享 code、global、heap 和 mappings，各自拥有寄存器、栈与 TID。因此传递一个 `SensorData *` 很便宜，代价是 data race、lock contention、false sharing 和同一故障域。

Process 是资源隔离容器，各自有页表和虚拟地址空间。两个进程即使都打印 `0x7ffd...`，MMU 也可将它们翻译到不同物理页。普通 pointer 只在创建它的地址空间有意义。跨进程要么通过系统调用把定义好的 wire bytes 放入 pipe/socket kernel buffer，要么让两端 `mmap(MAP_SHARED)` 同一对象，并另行设计同步和 ownership。

映射到 ROS2：两个 composable node 在同一进程可使用 intra-process 优化；两个独立进程要走 RMW transport/SHM。合并进程可能减少 serialize/copy，但会改变 executor 并发、崩溃隔离和资源争用。

### 深入追问

- **ROS2 node 与 process 一一对应吗？** 不对应；一个进程可有多个 node，一个 node 的 callback 又可由多个 executor worker 执行。
- **共享地址空间是否意味着无复制？** 不一定；应用容器、ownership 扇出和 API 仍可能复制。
- **如何验证？** 运行 `thread_shared_state` 与 `process_memory_isolation`，比较 PID/TID/address/final value。

## Q2. fork 后为什么父子变量地址相同，child 修改后 parent 却不变？

### 30 秒回答

地址是进程内的虚拟地址。`fork` 复制父进程的地址空间视图，父子初始 mapping 相同，常通过 copy-on-write 共用物理页；一方写入触发 page fault，内核复制该页并更新写入方页表，所以虚拟地址仍相同，变量内容已经独立。

### 2 分钟回答

CPU 访问 virtual address 时经当前进程页表转换。`fork` 后父子拥有不同页表，但内核为节省成本，可让对应 PTE 暂时指向同一只读物理页并标记 COW。当 child 执行写指令，CPU 产生 protection fault；内核分配新页、复制旧内容、让 child PTE 指向新页并恢复写权限。Parent PTE 仍指向原页。

所以三个命题可同时成立：初始数据相同、打印的 virtual address 相同、写后内容不同。只有显式 `MAP_SHARED` mapping 或其他 IPC 才让更新具备跨进程可见语义；即使共享，还需同步建立 happens-before。

### 深入追问

- **COW 是否意味着 fork 后永远不复制？** 否；写时按页复制，页表和进程元数据也有成本。
- **可以用 `/proc/PID/maps` 证明物理页相同吗？** 不能；maps 主要显示虚拟映射，物理页证据需更底层接口且常受权限限制。
- **exec 后 PID 会变吗？** 不会；exec 替换进程映像，保留 PID，并按 FD_CLOEXEC 处理 fd。

## Q3. 系统调用是否必然发生上下文切换？

### 30 秒回答

不必然。Syscall 必然涉及从用户态进入内核态的 privilege/mode transition，但若内核很快完成并返回，CPU 仍可继续执行同一线程。只有调度器改为运行另一个 thread/process 才是 scheduling context switch；blocking read、抢占或 page fault 可能触发它。

### 2 分钟回答

要分三件事：

1. function call 只改变用户态控制流；
2. syscall 通过受控入口进入 kernel mode，内核代表当前 thread 操作 fd、网络或 memory；
3. context switch 保存当前调度实体状态并恢复另一个实体。

例如向有空间的 pipe 写少量数据：write 进入内核、复制并返回，可能没有换线程。空 pipe 上 blocking read：内核把当前 thread 置为等待，调度其他 thread，数据到来后再唤醒并在某个时刻重新调度。性能成本还包括 cache/TLB/branch predictor 扰动，所以不能背一个固定“context switch 是 N 微秒”。

这也解释 ROS2：DDS 已把 subscription 置 ready，不代表 executor thread 立即获得 CPU；callback scheduling delay 独立于 transport latency。

### 深入追问

- **怎么观察？** `strace -ttT` 看 syscall duration，`perf stat -e context-switches` 看计数；二者口径不同。
- **futex 每次 mutex lock 都调用吗？** 常见 mutex 无争用路径在用户态，争用时才进入 futex slow path。
- **strace duration 等于 kernel execution time 吗？** 阻塞期间可能包含等待和被调度出去的墙上时间。

## Q4. Pipe 的核心语义是什么？EOF 何时出现？

### 30 秒回答

匿名 pipe 是由内核缓冲的有序字节流，通常通过 fork 继承 fd。它没有应用消息边界，容量有限，空读和满写可阻塞。当 buffer 已耗尽且所有进程持有的 write end 都关闭后，read 才返回 0 表示 EOF；遗漏任何 write fd 都会让 reader 继续等待。

### 2 分钟回答

`pipe2` 给出 read/write 两个 file descriptor。Writer 的 `write` 把字节移入有限 kernel pipe buffer，reader 的 `read` 移出。应用必须处理 partial read/write、`EINTR`、non-blocking `EAGAIN` 和 reader 消失时的 `SIGPIPE/EPIPE`。

`PIPE_BUF` 只保证在规定大小内，多 writer 的单次 write 不会与另一个 writer 交错；它不保证 reader 一次 read 恰好得到一整条业务消息。EOF 是引用生命周期语义：必须没有任何 open write end。fork 后最常见 bug 是 child 或 parent 继承了不需要的 write end却没关闭。

本实验使用 `O_CLOEXEC` 默认防泄漏，只为 B 需要的 read fd 清除 `FD_CLOEXEC`，并由 A 关闭 write end触发 EOF。

### 深入追问

- **Pipe 满了怎样？** Blocking writer 等待，non-blocking 得到 EAGAIN；具体 capacity 不应硬编码。
- **为什么 ROS2 topic 不直接用 pipe？** 缺少 dynamic discovery、fan-out、cross-host、type/QoS/history。
- **如何看 EOF 证据？** `strace -ff` 中最后一个 read 返回 0，同时检查所有 close。

## Q5. 为什么 TCP/pipe 必须做 framing？一次 send 对应一次 recv 吗？

### 30 秒回答

TCP 和 pipe 提供 byte stream，不保留应用 write/send 边界。一次 32-byte send 可能被多次 recv，一次 recv 也可能合并多次 send。协议必须使用固定长度、长度前缀或 delimiter，并循环处理 partial I/O；否则会把 transport 分段误判为丢消息。

### 2 分钟回答

Stream 的契约是有序字节序列。内核会受发送缓冲、MSS、Nagle、调度、接收缓冲和调用 buffer size 影响，任意切分/合并。正确接收端维护状态：

- fixed-size 协议：循环直到收满 N；
- length-prefix：先收固定 header、校验上限，再收 payload；
- delimiter：处理 delimiter 跨 recv 和 escaping。

发送端也要循环，因为 write/send 可以 partial，尤其 non-blocking、信号或资源压力下。本实验的 `write_all/send_all/read_exact` 处理这些情况；clean EOF 只能发生在新 frame 尚未读任何 byte 时，frame 中途 EOF 是协议错误。

UDP/UDS datagram 保留 record boundary，但 buffer 太小可能截断整条 datagram，多余部分不会留给下一次 recv。

### 深入追问

- **TCP 能否丢 application message？** 若连接最终成功可靠传完则字节完整；应用进程崩溃、超时、关闭和协议错误仍可造成业务失败。
- **DDS sample 在 UDP 上如何超过 MTU？** RTPS/DDS vendor 可分成 DATA_FRAG 等 fragment 并重组。
- **为什么长度字段要校验上限？** 防止损坏/恶意 header 触发无界 allocation。

## Q6. Unix Domain Socket 与 TCP loopback 有何区别？为什么 ROS2 不只用 UDS？

### 30 秒回答

UDS 是本机 socket IPC，用 pathname/abstract name 定位，不走 IP 路由和物理 NIC，可选 stream/datagram/seqpacket。TCP loopback 仍走 IP/TCP stack。UDS 只解决本机 transport；ROS2 还要跨机器、分布式发现、typed topic、N 对 M、QoS/history 和 vendor 互操作，所以不能由一个 UDS 替代 DDS/RMW。

### 2 分钟回答

UDS 与网络 socket 都使用 fd/socket API和内核 queue，但 address family 与协议路径不同。AF_UNIX pathname 是 endpoint 名称，不是把 payload 写入磁盘文件。SOCK_DGRAM 保留边界，SOCK_STREAM 与 TCP 一样要求 framing。UDS 可做 credential passing 和 `SCM_RIGHTS` fd passing，非常适合本机 daemon。

ROS2 的设计目标比 transport 大：发现 participant/endpoint，匹配 topic/type/QoS，管理 writer/reader history，实现 reliability/durability，并让同一 application API 适用于同机和跨机。DDS vendor 内部可以选 UDP、SHM 或其他 transport；UDS 可能出现在某个实现内部，但不是 ROS graph 的完整语义。

### 深入追问

- **UDS datagram 一定不丢吗？** 不是；queue 有界，发送/接收和进程退出仍有失败。
- **为什么 pathname 会 stale？** Socket 进程退出不必自动删除文件系统名字；owner 要 unlink。
- **tcpdump 能看 UDS 吗？** 常规 IP packet capture 看不到；用 strace/ss/lsof。

## Q7. TCP、UDP 和 DDS 如何比较？机器人为什么常偏向 UDP？

### 30 秒回答

TCP 是可靠有序 byte stream；UDP 是尽力、有边界的 datagram；DDS 是更高层的 data-centric middleware，可在 UDP/SHM等 transport 上提供 discovery、type、history 和按 endpoint 配置的 QoS。机器人传感器常重 freshness：旧帧丢了可以用新帧，不希望 connection-wide 重传阻塞后续数据，所以常用 UDP/BEST_EFFORT；这不是说 UDP 永远更快或控制都该用 UDP。

### 2 分钟回答

TCP 将可靠性、顺序和拥塞控制施加到整个 byte stream。丢一段时，后续字节出现 head-of-line blocking。对地图、文件或 broker/WAN，这可能很合适。

UDP 保留 datagram，无连接建立，支持 multicast，但不保证到达/顺序/唯一。DDS/RTPS 可在 UDP 上按 DataWriter/DataReader 关系实现 heartbeat、ACKNACK、retransmission，也可对 camera/lidar 使用 BEST_EFFORT，只保留新样本。这样 reliability policy 与数据语义更接近，而不是所有 topic 共享一个 TCP stream fate。

选择仍取决于 message size、loss、MTU、Wi-Fi、security、implementation 和 business safety。Control command 常同时需要 delivery、freshness deadline、liveliness 与 fail-safe，不能只选择“RELIABLE”就结束设计。

### 深入追问

- **RELIABLE 是否永不丢？** 进程崩溃、资源限制、超时、history overwrite 和网络长期分区仍可失败；语义需结合 QoS/vendor。
- **大 UDP sample 风险？** fragmentation 增加丢失放大和重组资源压力。
- **什么时候 TCP 更合适？** WAN/broker、完整 byte stream、生态/防火墙优先且可接受 HOL 时。

## Q8. `send()/publish()` 返回成功能证明什么？

### 30 秒回答

通常只能证明当前 API 按其本地契约接受/处理了数据。Socket send 成功常表示字节进入本机内核发送路径；不能证明 NIC 已发送、远端收到、DDS reader 入 cache、subscription take 或 callback 执行。`publish()` 的返回点还受 RMW/vendor 同步/异步模式和资源策略影响。

### 2 分钟回答

端到端链路有多个独立 acknowledgement 概念：

1. application 调用返回；
2. middleware writer 接受 sample；
3. kernel send buffer 接受 bytes；
4. NIC 发出；
5. remote transport 收到；
6. reliable reader ACK；
7. reader history 入样；
8. executor take；
9. callback start/end；
10. 业务层处理成功。

这些不能互相替代。TCP ACK 也主要确认对端协议栈接收字节，不等于应用 callback。DDS RELIABLE acknowledgement 的具体意义也不是业务事务提交。需要端到端业务确认时，应设计 request/response、sequence/state machine 或应用 ACK，并有 timeout/idempotency。

### 深入追问

- **如何证明 callback 执行？** 应用 event/tracepoint/业务 ACK；抓包只能证明更早阶段。
- **`rmw_publish` tracepoint 是发包时刻吗？** 不一定，异步 writer 可稍后 transport。
- **为什么日志“publish ok”仍可能无数据？** endpoint 未匹配、QoS、locator、queue/resource、subscriber scheduling 都在后面。

## Q9. Shared memory 为什么快？成本和风险是什么？

### 30 秒回答

两个进程把虚拟地址映射到同一 backing pages，可以避免大 payload 经过 user→kernel buffer→user 的传统复制和部分协议栈开销，所以大数据常更快。但仍有写内存、cache coherence、同步、通知、调度、slot ownership、序列化可能性、内存带宽和 crash cleanup；小消息时管理成本可能抵消收益。

### 2 分钟回答

`shm_open + ftruncate + mmap(MAP_SHARED)` 建立共同 backing。Writer 写 payload，发布 generation/slot；reader 被 condition/futex/event 通知后读取。高性能设计通常预分配 pool，使用 loaned slot 状态机，避免 allocation 和 payload 搬运。

困难集中在控制面：

- reader 不能读半写 sample；
- writer 不能覆盖仍被 reader 持有的 slot；
- 多 reader 需要引用/lease；
- 慢 reader 要 backpressure、drop 还是 detach；
- crash 时 robust mutex、stale segment、orphan slot 如何恢复；
- cache line ping-pong/false sharing/NUMA；
- 权限、版本/layout 与 container IPC namespace。

所以结论必须按 payload、频率、fan-out、硬件和实现实测，而不是“共享内存零成本”。

### 深入追问

- **两端 mmap virtual address 要相同吗？** 不需要；各自页表可映射同一 backing。
- **`shm_unlink` 后 mapping 怎样？** 名字删除，已有 fd/mapping 一般继续有效直到最后引用释放。
- **本实验为什么有 flock 和 mutex 两种锁？** flock 保护 initialization publication；process-shared mutex/cond 保护 sample ownership。

## Q10. Intra-process、Shared Memory、Loaned Message 与 Zero Copy 如何区别？

### 30 秒回答

Intra-process 是同一进程内绕开常规 inter-process 路径；SHM transport 是不同进程共享 backing pages；loaned message 是 middleware/pool 借 buffer 给应用；zero copy 是对一条明确路径“某次复制被消除”的性质。四者有关但不等价，启用一个不能证明端到端零复制。

### 2 分钟回答

- **Intra-process**：rclcpp 在同进程 publisher/subscriber 间可转移 `unique_ptr` 或共享 ownership，可能避免 serialize/RMW，但多订阅者扇出会影响所有权；
- **Shared-memory transport**：DDS vendor用共享 segment 在进程间传样本，仍可能在 ROS object、serialized history、reader take 等位置复制；
- **Loaned message**：buffer 由 middleware/allocator pool 提供，application 写/读后归还；受 RMW、type boundedness 和 API 能力限制；
- **Zero copy**：必须说清从哪里到哪里。例如“publisher application 到 writer history 不复制”不等于“sensor DMA 到 perception algorithm 无复制”。

评估时画 copy ledger：分配者、owner、copy source/destination、serialize、cache transition、归还点和 fallback。

### 深入追问

- **SHM 一定不序列化吗？** 不一定，取决于 vendor data-sharing/transport/type/path。
- **Loaned message 一定跨进程？** 不一定；loan 是 ownership API，不定义 transport。
- **Humble 所有 RMW 都支持 loaning 吗？** 不应假设；运行时探测并对具体 RMW/消息类型说明。

## Q11. 慢消费者会让系统发生什么？

### 30 秒回答

取决于通道和 policy。Pipe/TCP 的有限 buffer 满后可把 backpressure 传给 writer并形成 latency backlog；UDP queue 满常丢 datagram；single-slot SHM 可阻塞 writer、覆盖旧 slot 或丢新样本；DDS 则由 history/depth/reliability/resource limits 决定。必须同时看 receive Hz、sequence gap 和 message age。

### 2 分钟回答

慢 consumer 的核心不是“会不会丢”一个问题，而是资源到上限时选择：

- **block/backpressure**：保持完整但 producer 被拖慢，尾延迟扩大；
- **queue**：短期吸收 burst，长期速率不平衡仍会满；
- **drop oldest/latest**：牺牲 completeness 保 freshness 或相反；
- **overwrite latest-value**：consumer 读取最新 state，中间 generation 跳过；
- **disconnect/degrade**：保护关键路径。

TCP 连接级 flow control 可让 write 最终阻塞；UDP send 成功但 receiver kernel queue 可能丢；本课程 SHM baseline 等 ack 才重用 slot，明确选择 backpressure。ROS2 QoS depth 主要描述 DDS history，不等于 Linux socket queue，也不能只看 `ros2 topic hz` 判断 backlog。

### 深入追问

- **100 Hz 收到 100 Hz 是否无问题？** 可能稳定处理一秒前旧消息；必须看 age。
- **Camera 更适合哪种？** 常偏向浅 depth/latest freshness，但要由算法容错决定。
- **Control 命令呢？** 需要 deadline/fail-safe，旧命令完整到达可能比丢弃危险。

## Q12. 你会怎样定位“发送端正常，接收端没数据”？

### 30 秒回答

按边界缩小：确认 producer sequence 真在推进；确认进程/线程存活；用 fd/socket/SHM 工具确认 endpoint；用 strace 看 send/recv/blocking；IP 路径用双方 tcpdump；最后对照 receiver sequence/callback trace。每一步说明能排除什么，不能把 send 成功或抓到 packet 当作 callback 成功。

### 2 分钟回答

我会先定义“正常”的证据和故障层：

1. **Application**：source sequence、timestamp、错误码；
2. **Process/thread**：`ps -L`、CPU、thread state；
3. **IPC endpoint**：`lsof`、`ss -x/ltnup`、`/proc/PID/maps`；
4. **Syscall**：`strace -ff -ttT` 看 fd、返回码和阻塞；
5. **Network**：只对 IP path 在正确 interface/namespace 双端抓包；
6. **Queue/resource**：Recv-Q/Send-Q、system counters、message age/gap；
7. **ROS2 后续层**：RMW/DDS match、QoS、WaitSet、Executor 和 callback trace。

若 UDS 无数据，tcpdump 没包是预期；若 UDP sender 在 receiver bind 前发送，sendto 可能成功但没有未来队列；若 pipe reader不 EOF，要找泄漏 write fd；若 SHM reader卡住，要检查 initialization/version、mutex owner、generation predicate。

### 深入追问

- **strace 显示 futex wait 是死锁吗？** 不足以证明，可能正常等待 condition；需要 predicate/owner/进展证据。
- **packet 到达 receiver NIC 后下一步？** kernel receive/socket queue、DDS reader history、take、executor scheduling。
- **为什么先最小侵入观测？** printf/strace/topic echo 都会改变 timing 或增加 subscriber，需要记录观察者效应。

## Q13. 为什么不能直接发送 native C++ struct？

### 30 秒回答

Native struct 不是稳定 wire protocol：可能有 padding/alignment、主机字节序、ABI/compiler/version 差异；若含 pointer、`std::string/vector`，内部地址只对当前进程有效。应定义 versioned schema和 serialization，校验长度/范围。Stage 01 手写 big-endian record，ROS2 后续用 ROSIDL typesupport/CDR。

### 2 分钟回答

即使一个 struct 当前是 trivially copyable，也要审查：

- field layout/padding 是否固定；
- integer 和 float endianness；
- type width；
- alignment；
- schema evolution；
- untrusted length；
- 架构/编译器 ABI；
- pointer/allocator/vtable；
- 校验、magic 与 version。

本实验协议明确 32-byte offsets：magic/version/flags/sequence/monotonic timestamp/float bit patterns，整数统一 big-endian。Shared-memory region 可直接共享同 ABI 对象，是因为限制在同主机同版本两端，仍用 magic/version/size 检查和同步；它不是跨机器协议。

ROS2 的 `.msg/.idl` 经 rosidl 生成 type support，DDS 常用 CDR serialization解决 alignment/endianness/schema；具体复制和 serialize 位置在 RMW/vendor 路径验证。

### 深入追问

- **`memcpy` 一个 POD struct 在同机构建中可以吗？** 在严格约束 ABI/version 时可作为内部协议，但要显式写下约束，不能自动外推。
- **Timestamp 为什么用 monotonic？** 测 duration 不受墙上时间校正；跨主机 monotonic epoch 不同，不能直接相减。
- **Version 不匹配怎么办？** 明确拒绝、协商或兼容转换，不能默默按本地 layout 解释。

## Stage 01 口述验收

- [ ] 13 题的 30 秒版本能在 8 分钟内完成。
- [ ] 每题至少能指向一个仓库实验或 Linux 观测证据。
- [ ] 不使用“总是”“一定”“零成本”等无前提绝对句。
- [ ] 能把任意一题连接到后续 DDS/Executor/QoS/Zero Copy。
