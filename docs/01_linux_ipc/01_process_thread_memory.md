# Chapter 1 — Process、Thread、Virtual Memory 与 Context Switch

重要度：⭐⭐⭐

## 1. 为什么需要先讲这些

ROS2 node 可以是一个进程，也可以由 composition 把多个 node 放进同一进程。Executor 又可能有一个或多个线程。选择一变，数据是否必须跨地址空间、callback 是否并发、崩溃是否相互隔离、能否传递对象所有权都会变化。

“node”和“process”不是同义词，“callback”和“thread”也不是一一对应。

## 2. 生动例子

同一进程的线程像共用一张工程图纸：指向图纸第 100 行的指针对所有线程有同样意义。不同进程各有一张编号相同的图纸；“第 100 行”只是各自地址空间中的位置，不代表同一张纸。

Linux 可以让两张图纸的某几页指向同一物理页，这就是 shared mapping，但必须显式建立。

## 3. 核心理论

### 3.1 Process

进程是资源与隔离的主要容器，拥有自己的虚拟地址空间、页表视图、文件描述符表、信号处理状态等。两个进程的虚拟地址都可能显示 `0x7ffd...`，但 MMU 经不同页表翻译后可指向不同物理页。

`fork()` 初始时通常通过 copy-on-write 共享物理页：父子页表都指向只读的同一物理页；任一方写入触发 page fault，内核复制该页并更新写入方页表。因此“刚 fork 时共享物理页”不等于“变量更新对双方可见”。

### 3.2 Thread

同一进程的线程共享代码、global/static、heap 和 memory mappings；每个线程有自己的寄存器、栈、调度状态与 TID。多个线程访问同一非原子变量且至少一个写入，如果没有 happens-before，就是 C++ data race，行为未定义。

共享地址空间带来低成本传递，也扩大了并发错误和故障影响：一个野指针可以损坏整个进程。

### 3.3 User space 与 kernel space

应用普通指令在 user mode 执行，不能直接操作页表、NIC 或任意物理内存。`read/write/send/mmap/futex` 等系统调用切换到内核执行受控操作，再返回用户态。

“进入内核”不是必然发生进程调度；系统调用返回也可能仍由原线程继续执行。反过来，线程可因时间片、抢占、page fault 或等待 I/O 被切走。

### 3.4 Context switch

调度器从线程 A 切到线程 B，需要保存/恢复寄存器和调度状态，并可能扰动 cache/TLB/branch predictor。进程切换往往比同进程线程切换带来更多地址空间相关成本，但现代硬件、PCID、工作集和调度路径使“固定纳秒数”没有普适意义。

重要区分：

- **mode switch**：用户态进入内核态；
- **thread context switch**：CPU 改为执行另一个调度实体；
- 一次 syscall 可能没有 thread switch；
- 一次阻塞 read 往往导致当前 thread 被调度出去。

## 4. ROS2 中如何映射

- Components + intra-process：多个 node 可共享一个进程，rclcpp 有机会传递 shared/unique ownership；
- 普通两个 node 进程：指针无跨进程意义，必须经 RMW transport 或 shared mapping；
- SingleThreadedExecutor：多个 callback 通常在同一 executor thread 串行；
- MultiThreadedExecutor：共享 node state 时必须遵守 callback-group 和 C++ 同步；
- DDS vendor 还有自己的 discovery/receive/send threads，它们与 executor threads 是不同集合。

## 5. Linux 底层发生了什么

### Thread 实验

`thread_shared_state` 创建两个 `std::thread`，它们打印：

- 相同 shared object virtual address；
- 同一 PID；
- 不同 kernel TID；
- 在 mutex 保护下更新同一状态。

mutex 竞争在无争用时可能只走用户态 fast path；需要睡眠/唤醒时通常借助 `futex`。

### Process 实验

`process_memory_isolation` 在 `fork` 前创建变量。父子打印相同数值和虚拟地址，child 写为 99 后，parent 仍为 42。这是 COW 与独立页表语义的可见结果。

## 6. 动手实验

### 实验猜想

先写答案：

1. 两线程打印的 object address 是否相同？
2. fork 后父子地址是否一定不同？
3. child 修改后 parent 会看到 99 吗？
4. `getpid()` 和 Linux TID 在两个 worker 中分别怎样？

### 运行

```bash
./scripts/build_stage1.sh
build/stage1/thread_shared_state
build/stage1/process_memory_isolation
```

预期输出的数值和顺序可能不同，但不变量如下：

```text
thread ... pid=100 tid=101 object_address=0x...
thread ... pid=100 tid=102 object_address=0x...
final_counter=200000 expected=200000

before fork ... value=42 address=0x...
child ... changed value=99 address=0x...
parent ... value_after_child_exit=42 address=0x...
```

不要把 stdout 顺序当同步证据。验证的是 PID/TID、地址关系和最终值。

## 7. 如何观察系统

增加 `--hold-ms 5000` 让进程停留，另开终端：

```bash
build/stage1/thread_shared_state --hold-ms 5000 &
pid=$!
ps -L -p "$pid" -o pid,tid,psr,stat,comm
sed -n '1,40p' "/proc/$pid/status"
cat "/proc/$pid/task/$pid/status" | head
wait "$pid"
```

系统调用与 clone/fork：

```bash
strace -ff -ttT -e trace=clone,futex,nanosleep \
  build/stage1/thread_shared_state

strace -ff -ttT -e trace=clone,fork,vfork,wait4,write \
  build/stage1/process_memory_isolation
```

上下文切换计数：

```bash
perf stat -e context-switches,cpu-migrations,page-faults \
  build/stage1/thread_shared_state
```

计数受系统其他负载影响。做对照实验时至少重复多轮、固定 workload，不要从一次数据推导绝对成本。

## 8. 常见坑

- 认为“相同地址”证明共享同一变量：跨进程不成立；
- 去掉 mutex 后看到结果偶尔正确，就认为没有 data race；
- 把 PID、pthread id、Linux TID 混为一谈；
- 把 syscall/mode switch 和 scheduler context switch 混为一谈；
- 认为 composition 必然更快：它减少某些边界，但也改变 failure isolation、调度和 ownership；
- 用 `system_clock` 测耗时：墙上时钟可被校时调整，持续时间优先使用 monotonic/steady clock。

## 9. 真实机器人案例

相机 driver、perception 和 visualization 若放在一个 component container，可减少大图像传递成本；但 perception 的野指针可能带走 driver，慢 callback 也可能争用同一 executor。若拆进程，隔离更好，却必须承担 serialization/transport/copy。工程选型是在性能、隔离、调度和可运维性之间权衡。

**【求职价值】** 面试官借这个问题判断你能否把 ROS2 composition、intra-process 和 Executor 并发落到 OS 模型，而不是把 node、process、thread 当同义词。

## 10. 面试题

1. thread communication 与 process communication 的根本区别是什么？
2. fork 后父子变量为什么地址相同但值可独立变化？
3. 系统调用是否必然产生上下文切换？
4. ROS2 node 和 process 是一一对应吗？
5. MultiThreadedExecutor 为什么会暴露更多 data race？

三层答案见[Stage 01 面试题](../../interview/stage1_linux_ipc.md)。

## 11. 🔎 Source Dive 与本章总结

先读 `man 2 fork`、`man 7 pthreads`、`man 2 futex`、`man 5 proc_pid_status`。源码观察从本实验的系统调用边界开始，不需要一上来读内核 scheduler。

本章必须留下的模型：

> 线程默认共享进程地址空间；进程默认隔离虚拟地址空间。跨进程传数据必须复制到某种内核/传输通道，或显式建立共享映射。数据 ready 与线程获得 CPU 是两件事。
