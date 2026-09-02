# Chapter 5 — POSIX Shared Memory：共享页不等于自动同步

重要度：⭐⭐⭐

## 1. 为什么 Shared Memory 更快

Socket/pipe 通常要把 payload 从 producer user buffer 移入内核管理的缓冲，再移到 consumer user buffer。Shared memory 让两个进程的虚拟地址分别映射到同一组物理页；producer 写入后 consumer 可直接读取共享页，避免大 payload 沿传统 stream/datagram 路径再次搬运。

它省的是某些数据复制与协议栈工作，不是让 CPU、内存、同步和调度成本消失。

## 2. 生动例子

Socket 是把 5 MB 箱子交给快递中转，再送入隔壁房间。Shared memory 是两个房间共享一间仓库：producer 把箱子放在货架，通知 consumer 货架号；consumer 读取后归还货架。

如果没有“写完”标志、锁、generation 或 ownership 规则，consumer 可能在 producer 只放了一半时开箱；若 producer 覆盖尚未读完的货架，也会得到混合数据。

## 3. 核心理论

### 3.1 建立共享映射

```text
writer process                           reader process
shm_open(name, O_CREAT|O_RDWR)          shm_open(name, O_RDWR)
ftruncate(fd, region_size)               fstat(fd)
mmap(..., MAP_SHARED, fd, 0)             mmap(..., MAP_SHARED, fd, 0)
       │                                      │
       └──── page tables map same backing ────┘
```

- `shm_open` 创建/打开具名 POSIX shared-memory object；
- `ftruncate` 设置大小；在此之前越界访问可能 `SIGBUS`；
- `mmap(... MAP_SHARED ...)` 建立映射；
- `munmap/close` 释放当前进程引用；
- `shm_unlink` 删除名字；已有 mappings 可继续存在，直到最后引用消失。

### 3.2 Synchronization

共享物理页只解决“双方能访问”，不解决：

- producer 写完了吗？
- consumer 已读完吗？
- 谁可覆盖？
- 多个 producer 谁先写？
- 进程崩溃时锁/slot 怎么恢复？

本实验用设置了 `PTHREAD_PROCESS_SHARED` 的 `pthread_mutex_t` 和 `pthread_cond_t`。默认 pthread mutex/condition 只面向同进程线程，不能把未设置 pshared 的对象直接放入共享区就假定跨进程有效。

Condition variable 必须配合 predicate while-loop：

```cpp
while (generation == last_seen && !shutdown) {
  pthread_cond_wait(&updated, &mutex);
}
```

因为存在 spurious wakeup，且从 wake 到重新持锁之间状态可能再次变化。

**Semaphore 与本实验选择**：POSIX semaphore（具名 `sem_open`，或共享区内以 `pshared != 0` 初始化的 `sem_init`）维护可消费的计数，适合表达“有 N 个 slot/item”。Mutex 表达互斥 ownership，condition variable 表达“某个受 mutex 保护的 predicate 可能改变”。Semaphore 不是数据一致性的自动替代品：payload publication、slot generation、退出和 crash recovery 仍需协议。本实验用 mutex + condition，是因为要等待 `reader_ready`、`generation consumed` 和 `shutdown` 三个显式 predicate，而不只是一个计数。

### 3.3 Memory ordering 与 cache coherence

多核 CPU 各有 cache。硬件 coherence 使同一 cache line 的共享读写最终遵守一致性协议，但：

- cache-line ownership 会在核之间迁移；
- false sharing 会让无关字段互相抖动；
- coherence 不等于 C++ 无 data race；
- mutex/atomic 仍要建立 happens-before；
- 大 payload 仍消耗 memory bandwidth 和 cache capacity；
- NUMA 跨节点访问可能更贵。

### 3.4 Ownership

真正的高吞吐 SHM 中间件通常使用 pool/ring/slot：

```text
FREE → LOANED_TO_WRITER → PUBLISHED → HELD_BY_READER(S) → FREE
```

metadata/通知可以很小，payload 留在 slot。但多订阅者引用计数、慢 reader、crash recovery、bounded memory 和 ABA/generation 都要设计。

## 4. 与 ROS2 术语的边界

| 术语 | 本质 |
|---|---|
| Intra-process | 同一进程内的 rclcpp 优化，可能转交 C++ object ownership |
| POSIX SHM | OS 提供共享 backing pages |
| DDS SHM transport | DDS vendor 在共享段上实现 discovery/matching 后的数据 transport |
| Loaned message | middleware/pool 将 buffer 临时借给 application |
| Zero copy | 某条定义清楚的路径上消除特定 copy |

两进程使用 DDS SHM transport 仍可能在 ROS object ↔ serialized sample、writer history 或 reader API 处复制。“SHM 已启用”不证明端到端 zero copy。

## 5. 本实验的工程设计

共享区包含：

```cpp
struct SensorData {
  std::uint64_t timestamp_ns;
  float x;
  float y;
  float z;
};

struct SharedRegion {
  magic/version;
  pthread_mutex_t mutex;
  pthread_cond_t updated;
  bool reader_ready;
  bool shutdown;
  std::uint64_t generation;
  SensorData sample;
};
```

设计要点：

- writer 是 object owner，使用 `O_CREAT|O_EXCL` 防止覆盖未知旧对象；
- writer 初始化期间持有 `flock(LOCK_EX)`，reader 用 `LOCK_SH` 检查；若它在 creator 的 `shm_open` 与加锁之间极短窗口抢先打开，会释放锁并在 deadline 内重试 size，绝不在 `ftruncate` 前 mmap；
- mutex/condition 显式设置 process-shared；
- writer 等待 reader_ready，有 timeout，避免永久挂起；
- generation 防止把重复 wakeup 当新样本；
- reader 在锁内复制小 SensorData 到本地，再解锁打印，避免持锁做 I/O；
- writer 发布 shutdown 并 unlink 名字；映射已打开时仍安全；
- RAII 负责 fd、mmap、mutex lock 和 owner cleanup。

这不是 lock-free zero-copy 实现；它是故意清晰、可验证的 single-writer/single-reader baseline。

## 6. 动手实验

### 实验猜想

1. writer 与 reader 打印的 virtual address 是否必须相同？
2. 如果地址不同，它们能否仍指向相同 backing pages？
3. reader sleep 时 writer 继续覆盖单 slot，会发生丢 generation 还是排队？
4. writer 调用 shm_unlink 后，已经 mmap 的 reader 是否立刻崩溃？
5. mutex 保护 metadata 是否意味着 5 MB payload 没有 memory-bandwidth cost？

### 运行

终端 A：

```bash
build/stage1/shared_memory_writer \
  --name /ros2_comm_lab_sensor_demo \
  --count 10 \
  --interval-ms 20
```

writer 会先初始化并等待 reader。

终端 B：

```bash
build/stage1/shared_memory_reader \
  --name /ros2_comm_lab_sensor_demo \
  --count 10
```

预期：

```text
shm_writer name=/ros2_comm_lab_sensor_demo mapped_at=0x... waiting_for_reader=true
shm_reader name=/ros2_comm_lab_sensor_demo mapped_at=0x...
shm_rx generation=1 ... latency_us=...
...
shm_reader complete samples=10 skipped_generations=0
shm_writer complete samples=10 unlinked=true
```

两个 `mapped_at` 很可能不同；虚拟地址无须相同，页表可把它们映射到同一 backing object。

### 故障实验

用唯一 name 启动 writer 后强制终止，检查：

```bash
ls -l /dev/shm | rg ros2_comm_lab
```

stale object 会使下次 owner 使用 `O_EXCL` 明确失败。只清理自己确认属于实验的名字：

```bash
rm /dev/shm/ros2_comm_lab_sensor_demo
```

在生产系统应提供带 ownership/version/lease 的恢复工具，不应“启动时删掉同名所有对象”。

## 7. 如何观察系统

```bash
strace -ff -ttT \
  -e trace=openat,flock,ftruncate,mmap,munmap,futex,close,unlink \
  build/stage1/shared_memory_writer \
    --name /ros2_comm_lab_sensor_trace --count 1
```

另一个终端启动 reader。注意：

- glibc 的 `shm_open` 可表现为打开 `/dev/shm` 下对象；
- pthread wait/wake 常看到 futex；
- 每个 sample 不应出现 payload 的 sendto/recvfrom；
- 仍可能有用于 stdout 的 write syscall。

映射：

```bash
pidof shared_memory_writer
rg 'ros2_comm_lab_sensor' "/proc/$(pidof shared_memory_writer)/maps"
lsof -p "$(pidof shared_memory_writer)" | rg '/dev/shm'
```

## 8. 常见坑

- 先 mmap/访问、后 ftruncate；
- shared region 的版本/layout 不校验；
- 普通 `std::mutex` 或默认 pthread mutex 直接跨进程；
- condition variable 不配 predicate loop；
- 在持锁期间打印/磁盘 I/O；
- writer 覆盖 reader 正在使用的大 buffer；
- 进程崩溃留下 slot、锁、segment；
- false sharing；
- 把 volatile 当同步；
- 认为 cache coherence 等于免费；
- 认为一次 mmap 后就没有任何 copy；
- 在容器 private IPC namespace 中期待与 host/另一容器共享。

## 9. 真实机器人案例

4 路 1920×1080 RGB 30 FPS 的原始数据约为：

```text
1920 × 1080 × 3 bytes × 30 × 4 ≈ 746 MB/s
```

若每条链路多复制两次，内存流量会显著放大，cache 被污染，CPU 还要 serialize。SHM/loaned buffers 对相机/点云敏感，是因为 payload 大、频率高、扇出多。但 localization pose 只有几十到几百字节时，复杂 ownership 协议的收益可能小于同步与运维成本。

**【求职价值】** 大消息优化是中间件岗位的工程核心。高质量回答应能给出 copy ledger、slot ownership、slow reader policy 和 crash recovery，而不把“开 SHM”当成完成优化。

## 10. 面试题

1. Shared memory 为什么通常比 socket 更适合大消息？
2. 两进程 mmap 地址必须相同吗？
3. Shared memory 为什么仍需要 mutex/atomic？
4. `shm_unlink` 与 `munmap` 有何区别？
5. SHM、loaned message、zero copy 有何区别？
6. Shared memory 一定比 UDP 快吗？
7. 多 reader 下 buffer 何时可以回收？

## 11. 🔎 Source Dive 与本章总结

阅读 `man 3 shm_open`、`man 2 mmap`、`man 3 pthread_mutexattr_setpshared`、`man 3 pthread_condattr_setpshared`、`man 2 futex`、`man 2 flock`。随后在 Stage 07 对照 Fast DDS SHM 的 segment/port/buffer ownership。

> Shared memory 共享的是 backing pages，不是自动同步和自动 ownership。它可减少大 payload 的 transport copy，但仍付出内存带宽、cache coherence、通知、调度、资源回收和崩溃恢复成本。
