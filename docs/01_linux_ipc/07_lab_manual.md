# Chapter 7 — Stage 01 完整实验手册

本章把前六章变成一组可重复实验。不要跳过“猜想”：先写预期，再运行，最后用第二种观测面验证。

## 0. 实验约定

### 目标

- Ubuntu 22.04 / ROS2 Humble / C++17 是正式基线；
- 第一阶段 POSIX 代码也能在普通 Linux CMake 环境构建；
- 默认只使用 loopback、课程专用端口和课程命名的临时对象；
- `tcpdump/perf attach` 可能需要权限，核心 smoke test 不需要 root。

### 一键检查和构建

```bash
./tools/check_environment.sh
./scripts/build_stage1.sh
```

为什么先检查环境：`strace`、`ss`、`lsof`、`perf` 是可选观测工具，缺少它们不应被误判为源码构建失败。脚本最后运行 CTest；测试失败时不要继续做性能实验。

构建产物在 `build/stage1/`。正式 ROS2 Humble 容器验证：

```bash
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp/ros2_comm_lab_home \
  -v "$PWD:/workspace" \
  -w /workspace \
  ros2_humble_desktop_docker-ros2 \
  bash -lc '
    mkdir -p "$HOME"
    source /opt/ros/humble/setup.bash
    colcon build \
      --base-paths labs/01_linux_ipc \
      --packages-select linux_ipc_lab \
      --symlink-install \
      --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo
    ctest --test-dir build/linux_ipc_lab --output-on-failure
  '
```

为什么用非 root UID/GID：bind mount 中的 `build/install/log` 不会变成宿主机无法清理的 root-owned 文件。容器是一次性的；源码和构建产物通过 bind mount 保留。

---

## Lab 1 — 两个线程共享一个对象

### 实验猜想

运行前回答：

- 两个 worker 的 PID/TID 相同还是不同？
- `object_address` 相同吗？
- 如果删除 mutex，最终 counter 是否“只是偶尔少一点”，还是 C++ 层面已经 undefined behavior？


### 源码

- [`thread_shared_state.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/thread_shared_state.cpp)
- [`system.hpp`](../../labs/01_linux_ipc/linux_ipc_lab/include/linux_ipc_lab/system.hpp)

### 运行命令

```bash
build/stage1/thread_shared_state \
  --iterations 100000 \
  --hold-ms 0
```

参数目的：

- `iterations` 提供确定的共享写 workload；
- `hold-ms` 默认为 0；观测线程时设为 5000，让 `ps` 有时间采样。

### 预期输出

```text
thread name=A pid=... tid=... object_address=0x...
thread name=B pid=... tid=... object_address=0x...
final_counter=200000 expected=200000 process_pid=...
```

顺序不固定。不变量是同 PID、不同 TID、相同对象地址和最终 counter 等于 expected。

### 观察

```bash
build/stage1/thread_shared_state --iterations 100000 --hold-ms 5000 &
lab_pid=$!
ps -L -p "$lab_pid" -o pid,tid,psr,stat,pcpu,comm
ls "/proc/$lab_pid/task"
wait "$lab_pid"
```

`/proc/PID/task` 下每个 TID 有一个目录。`PSR` 只表示采样时/最近运行的 CPU，不保证线程始终固定在那里。

```bash
strace -ff -ttT -e trace=clone,futex \
  build/stage1/thread_shared_state --iterations 1000
```

你可能看不到每次 mutex lock 对应一次 futex：无争用 fast path 可只用用户态原子操作，发生争用/睡眠时才需要 futex。

### 结论检查

- [ ] 我没有把 “same PID” 与 “same TID” 混淆。
- [ ] 我知道共享地址空间省去 IPC，但没有省去同步。
- [ ] 我能解释为什么 MultiThreadedExecutor 会让 node 内状态需要同样的并发审查。

---

## Lab 2 — fork 后的进程变量隔离

### 实验猜想

最容易答错的是：父子打印的虚拟地址可能相同；child 写入后 parent 仍不变。这两点并不矛盾。

### 源码

[`process_memory_isolation.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/process_memory_isolation.cpp)

### 运行

```bash
build/stage1/process_memory_isolation --hold-ms 0
```

### 预期输出

```text
before_fork pid=... value=42 address=0x...
child pid=... parent_pid=... changed_value=99 address=0x...
parent pid=... child_pid=... value_after_child_exit=42 address=0x...
```

虚拟地址常相同，但这个现象不是实验断言；真正断言是 parent 仍为 42。

### 观察

```bash
strace -ff -ttT \
  -e trace=clone,fork,vfork,wait4,write \
  build/stage1/process_memory_isolation
```

在 Linux/glibc 上 `fork` 可能在 strace 中显示底层 `clone`。工具显示 syscall 实现入口，不改变 POSIX `fork` 语义。

### 进一步验证

```bash
build/stage1/process_memory_isolation --hold-ms 5000 &
parent_pid=$!
sleep 0.1
ps --ppid "$parent_pid" -o pid,ppid,stat,comm
sed -n '1,30p' "/proc/$parent_pid/maps"
wait "$parent_pid"
```

`maps` 给出 virtual mappings，不直接给出 COW 共享的物理页证据。需要 `pagemap`/内核工具时还受权限限制；本阶段不把地址文本当物理映射证明。

### 结论检查

- [ ] 我能解释 virtual address → page table → physical page。
- [ ] 我能解释 COW write fault。
- [ ] 我知道把 pointer 数值发送给另一个普通进程没有对象传递语义。

---

## Lab 3 — Anonymous pipe 跨 fork + exec

### 实验猜想

- `exec` 会创建新 PID 吗？
- child 为什么只有 read fd 能跨 `exec` 保留？
- parent 不关闭 write end 时，child 的 EOF 会怎样？
- 一次 32-byte write 是否保证一次 32-byte read？

### 源码

- [`pipe_process_a.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/pipe_process_a.cpp)
- [`pipe_process_b.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/pipe_process_b.cpp)
- [`wire_message.hpp`](../../labs/01_linux_ipc/linux_ipc_lab/include/linux_ipc_lab/wire_message.hpp)

### 运行

```bash
build/stage1/pipe_process_a --count 5 --interval-ms 20
```

A 调用 `pipe2(O_CLOEXEC)`，fork 后只为 read end 清除 `FD_CLOEXEC`，再 exec B。默认 close-on-exec 防止无关 fd 泄漏，显式授权唯一需要的端点。

### 预期

```text
pipe_process_a pid=... child_pid=... write_fd=...
pipe_process_b pid=... inherited_read_fd=...
pipe_rx seq=0 xyz=(0.00,0.25,-0.50) latency_us=...
...
pipe_process_b eof samples=5
pipe_process_a child_exit=0
```

### 观察

```bash
strace -ff -ttT -yy \
  -e trace=pipe2,clone,execve,fcntl,close,read,write,wait4 \
  build/stage1/pipe_process_a --count 2 --interval-ms 10
```

为什么加 `-ff`：fork 后继续跟踪每个进程，输出按 PID 分开/标识。`-yy` 尝试显示 fd 关联对象。通过 fd 区分 protocol pipe 的 write 和 stdout 的 write。

### 故障注入练习

复制一个临时分支，故意不在 parent 中 `write_end.reset()`，运行并观察 B 卡在哪个 read。使用 timeout 防止终端永久等待：

```bash
timeout 3s build/stage1/pipe_process_a --count 1
```

不要把故障版本留在主线。恢复 close 后确认 EOF test 通过。

### 结论检查

- [ ] 我能解释 EOF 取决于所有 write fd 的生命周期。
- [ ] 我能解释 partial read/write 和 framing。
- [ ] 我不会用 tcpdump 观察 pipe。

---

## Lab 4 — Unix Domain Datagram Socket

### 实验猜想

publisher 在 pathname 尚未 bind 时，数据会等待还是报错？UDS pathname 是 payload 文件吗？抓物理 NIC 能看到它吗？

### 源码

- [`unix_domain_subscriber.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/unix_domain_subscriber.cpp)
- [`unix_domain_publisher.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/unix_domain_publisher.cpp)
- [`socket_helpers.hpp`](../../labs/01_linux_ipc/linux_ipc_lab/include/linux_ipc_lab/socket_helpers.hpp)

### 运行

终端 A：

```bash
uds_path="/tmp/ros2_comm_lab_manual_$(id -u).sock"
build/stage1/unix_domain_subscriber \
  --path "$uds_path" --count 10 --timeout-ms 5000
```

终端 B：

```bash
uds_path="/tmp/ros2_comm_lab_manual_$(id -u).sock"
build/stage1/unix_domain_publisher \
  --path "$uds_path" --count 10 --interval-ms 20
```

Subscriber 必须先 bind。正常退出时 pathname 由 RAII 删除。

### 预期

```text
uds_subscriber path=/tmp/... count=10
uds_rx seq=0 ... latency_us=...
...
uds_subscriber complete samples=10 gaps=0 reordered_or_duplicate=0
```

### 观察

在 subscriber 等待期间：

```bash
ss -xap | rg ros2_comm_lab_manual
lsof -U | rg ros2_comm_lab_manual
ls -l "/tmp/ros2_comm_lab_manual_$(id -u).sock"
```

系统调用：

```bash
strace -ff -ttT -e trace=socket,bind,sendto,recvfrom,close,unlink \
  build/stage1/unix_domain_subscriber \
    --path "/tmp/ros2_comm_lab_trace_$(id -u).sock" \
    --count 1
```

另一个终端向 trace path 发送一条。Linux 上 libc `recv` 可能由 `recvfrom` syscall 表现。`tcpdump` 不适用，因为 AF_UNIX 不进入 IP packet path。

### Stale pathname

若进程被 SIGKILL，path 可能残留。先确认没有持有者：

```bash
lsof "/tmp/ros2_comm_lab_manual_$(id -u).sock"
ss -xap | rg ros2_comm_lab_manual
```

只删除确认属于本实验且无 owner 的精确路径。生产程序不能用 wildcard 清理其他服务 socket。

---

## Lab 5 — TCP byte stream 与 framing

### 实验猜想

一次 `send(32)` 对端是否必然一次 `read(32)`？若丢一个 IP packet，后续应用字节是否可以越过缺口先交付？

### 源码

- [`tcp_subscriber.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/tcp_subscriber.cpp)
- [`tcp_publisher.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/tcp_publisher.cpp)

### 运行

终端 A：

```bash
build/stage1/tcp_subscriber \
  --port 39001 --count 10 --timeout-ms 5000
```

终端 B：

```bash
build/stage1/tcp_publisher \
  --port 39001 --count 10 --interval-ms 20 --connect-timeout-ms 5000
```

Publisher 的有限 retry 只解决实验启动竞态；生产系统的重连还必须定义 backoff、session state 和 duplicate policy。

### 观察

```bash
ss -ltnp 'sport = :39001'
ss -tnp '( sport = :39001 or dport = :39001 )'
sudo tcpdump -i lo -nn -tttt 'tcp port 39001'
```

预期抓到 SYN/SYN-ACK/ACK、data/ACK 和 FIN。loopback capture 细节受 offload/kernel 影响，不要用 packet 数简单等同 send 次数。

```bash
strace -ff -ttT -e trace=socket,bind,listen,accept4,read,close \
  build/stage1/tcp_subscriber --count 2
```

`read_exact` 可能调用多次 read 才拼出一个 32-byte frame。即使这次 loopback 恰好每次都是 32，也不形成协议保证。

### 结论检查

- [ ] 我能说清 transport TCP 可靠的是 ordered bytes，不是应用 message。
- [ ] 我知道 `send` 返回不是远端 callback acknowledgement。
- [ ] 我能解释 head-of-line blocking。

---

## Lab 6 — UDP datagram 与丢失观测

### 实验猜想

先发送后 bind，旧 datagram 是否保留？一次 sendto 是否保留 receiver record boundary？无连接是否意味着无 kernel queue？

### 源码

- [`udp_subscriber.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/udp_subscriber.cpp)
- [`udp_publisher.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/udp_publisher.cpp)

### 运行

终端 A：

```bash
build/stage1/udp_subscriber \
  --port 39002 --count 10 --timeout-ms 5000
```

终端 B：

```bash
build/stage1/udp_publisher \
  --port 39002 --count 10 --interval-ms 20
```

### 观察

```bash
ss -lunp 'sport = :39002'
sudo tcpdump -i lo -nn -vv 'udp port 39002'
nstat -az | rg 'Udp|Ip'
```

Receiver 用 `MSG_TRUNC` 检查实际 datagram length；过大或过小都拒绝，避免 silent truncation 伪装成合法 frame。

### 启动顺序故障

先运行：

```bash
build/stage1/udp_publisher --count 3 --interval-ms 1
```

再运行期望 3 条的 subscriber。它会 timeout，因为发送时没有对应 receive socket queue。这是预期失败，不是程序随机坏了。

### 丢包扩展

Stage 09 会在专用 namespace 用 `tc netem`，避免污染宿主机。现在只提高发送量/减小处理速度，观察 sequence gap；不要直接在默认网卡上修改 qdisc。

### 结论检查

- [ ] 我能解释 UDP datagram boundary 和 best-effort。
- [ ] 我知道 DDS 可在 UDP 上另行实现可靠性。
- [ ] 我不会把小 loopback UDP 结果外推到 RTPS 大消息网络。

---

## Lab 7 — POSIX shared memory + 进程共享同步

### 实验猜想

- writer/reader 的 `mapped_at` 必须相同吗？
- writer `shm_unlink` 后，reader 既有 mapping 会怎样？
- 若没有 mutex/generation/ack，reader 可能看到哪些中间状态？
- reader 慢时，本实验选择丢样本还是 backpressure？

### 源码

- [`shared_memory_writer.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/shared_memory_writer.cpp)
- [`shared_memory_reader.cpp`](../../labs/01_linux_ipc/linux_ipc_lab/src/shared_memory_reader.cpp)
- [`shared_memory.hpp`](../../labs/01_linux_ipc/linux_ipc_lab/include/linux_ipc_lab/shared_memory.hpp)

### 运行

终端 A（writer 先创建并等待）：

```bash
build/stage1/shared_memory_writer \
  --name /ros2_comm_lab_sensor_manual \
  --count 10 \
  --interval-ms 20 \
  --timeout-ms 5000
```

终端 B：

```bash
build/stage1/shared_memory_reader \
  --name /ros2_comm_lab_sensor_manual \
  --count 10 \
  --timeout-ms 5000
```

### 预期

```text
shm_writer name=/ros2_comm_lab_sensor_manual mapped_at=0x... waiting_for_reader=true
shm_reader name=/ros2_comm_lab_sensor_manual mapped_at=0x...
shm_rx generation=1 sample seq=0 ... latency_us=...
...
shm_reader complete samples=10 skipped_generations=0
shm_writer complete samples=10 unlinked=true
```

地址可以不同。单 slot 协议要求 reader acknowledge generation N 后 writer 才覆盖，因此这个 baseline 体现 backpressure，不丢 generation。

### 观察

```bash
ls -l /dev/shm | rg ros2_comm_lab_sensor_manual
writer_pid=$(pgrep -n shared_memory_writer)
rg ros2_comm_lab_sensor_manual "/proc/$writer_pid/maps"
lsof -p "$writer_pid" | rg /dev/shm
```

```bash
strace -ff -ttT \
  -e trace=openat,flock,ftruncate,mmap,munmap,futex,close,unlink \
  build/stage1/shared_memory_writer \
    --name /ros2_comm_lab_sensor_trace \
    --count 1
```

另一个终端运行 reader。每条 sample 不会走 sendto/recv；mutex/condition 的 slow path 可能看到 futex。

### 初始化竞态为何用 flock

`shm_open(O_CREAT)` 返回后名字已经可见，但 writer 还没完成 `ftruncate/mmap/pthread init`。Writer 创建后立即请求 `LOCK_EX`；reader 打开后取 `LOCK_SH` 并检查 size。若 reader 恰好在 `shm_open` 与 writer 加锁之间抢先，它看到 size 未完成后释放共享锁并在 deadline 内重试，让 writer 获得独占锁；正常情况下 reader 等 writer 初始化结束后才检查 version/magic 并 mmap。这解决 initialization publication，不取代 sample mutex。

### Crash/stale object

强杀 writer 可能留下对象。确认没有 owner 后，只清理精确名字：

```bash
lsof /dev/shm/ros2_comm_lab_sensor_manual || true
rm /dev/shm/ros2_comm_lab_sensor_manual
```

课程默认正常退出会 `shm_unlink`。不要使用 `rm -rf /dev/shm` 或 wildcard。

### 结论检查

- [ ] 我能区分 shared backing page 与相同 virtual address。
- [ ] 我能解释 process-shared pthread attribute。
- [ ] 我能画 FREE/WRITING/PUBLISHED/READING/FREE ownership。
- [ ] 我不会把这个 SHM demo 称作端到端 zero copy。

---

## Lab 8 — 自动化回归和观测边界

### 运行

```bash
./scripts/run_stage1_smoke.sh
```

脚本逐项失败即停止；pair test 为 UDS 使用临时 pathname，为 TCP/UDP 使用进程相关端口，为 SHM 使用 UID/PID name。其目标是 correctness regression，不是 benchmark。

### 为什么 smoke test 不测性能

CI/容器的 CPU 调度、虚拟化与系统负载不稳定；将微秒阈值写入 correctness test 会制造 flaky test。性能阶段会：

- warm-up；
- 固定 payload 和 affinity；
- 多轮运行；
- 记录环境；
- 报告 distribution；
- 与 correctness assertions 分离。

### Stage 01 实验报告

完成后提交一页表：

| Mechanism | Boundary | Framing | Buffer/slot | Blocking evidence | Cleanup |
|---|---|---|---|---|---|
| Thread | none/process internal | object API | shared object | mutex/futex | join/RAII |
| Pipe | process + kernel | application | pipe buffer | read/write | close/EOF |
| UDS | process + kernel | datagram | socket queue | recv | close/unlink |
| TCP | process/machine + kernel | application | TCP buffers | connect/read/send | close/state |
| UDP | process/machine + kernel | datagram | UDP socket queue | recv | close |
| SHM | process, shared pages | region protocol | one slot | cond/futex | unmap/unlink |

并回答：

> 如果把 ROS2 两个 composable node 从两个进程合并为一个进程，哪些成本可能减少，哪些并发/隔离风险会增加？

## Stage 01 最终 Checklist

- [ ] 全部 4 个 CTest 通过。
- [ ] 每种 IPC 至少保存一份 strace/ss/lsof/proc/tcpdump 中适用的证据。
- [ ] 能指出不适用的工具及原因。
- [ ] 能解释 32-byte wire format 为什么不直接发送 struct。
- [ ] 能区分 stream framing、datagram boundary 和 shared slot protocol。
- [ ] 能解释 backpressure、drop 和 latest-value 三种慢消费者策略。
- [ ] 已完成[Stage 01 面试题](../../interview/stage1_linux_ipc.md)口述。
