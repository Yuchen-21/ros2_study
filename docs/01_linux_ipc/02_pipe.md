# Chapter 2 — Pipe（管道）：由内核托管的字节流

重要度：⭐⭐

> **初学者读法**：第一遍只读“0. 先记三件事”“2. 生动例子”和“6. 动手实验”。`PIPE_BUF`、`EINTR`、`FD_CLOEXEC` 属于第二遍细节，可先跳过并在[术语表](../00_glossary.md)查询。

## 0. 这一章先记三件事

1. Pipe 是 Linux 内核帮两个本机进程维护的一条“字节管道”。
2. 它只看见连续字节，不知道一条业务消息从哪里开始、在哪里结束。
3. 管道里已经没有数据，并且所有写入端都关闭后，读取方才会得到 EOF，也就是“以后不会再有数据”。

一句话场景：父进程已经能创建子进程，现在想把一串数据按顺序交给它，pipe 是最简单的办法之一。

## 1. 为什么需要 Pipe

父子进程常需要一条简单、单向、按顺序的本机数据通道。匿名 pipe 不需要名字或网络地址；父进程创建它后，子进程可以继承两端的文件描述符（file descriptor，简称 fd）。Shell 中的 `command_a | command_b` 就使用了同类思路。

## 2. 生动例子

把 pipe 想成墙内一根有容量的管：

```text
Process A user buffer
       │ write(fd)
       ▼
┌──────────────────────────┐
│ Linux kernel pipe buffer │  ← finite capacity
└──────────────────────────┘
       │ read(fd)
       ▼
Process B user buffer
```

它运输的是连续字节，不知道你的 `SensorData` 从哪里开始、在哪里结束。

## 3. 核心理论

`pipe2()` 会返回读取端和写入端两个 fd。核心语义：

- **stream**：没有 message boundary；
- **ordered**：单个字节流按写入次序读取；
- **blocking**：空 pipe 上 blocking read 等待；满 pipe 上 blocking write 可能等待；
- **EOF**：缓冲耗尽且系统中所有 write end 都已关闭时，`read` 返回 0；
- **atomicity**：不大于 `PIPE_BUF` 的单次 write 对多个 writer 不会与其他 write 交错；它不等于 reader 一次 read 必然取完整 record；
- **capacity**：有限且可由内核/配置决定，不能当无限队列；
- **backpressure**：慢 reader 最终可能让 writer 阻塞，non-blocking 则得到 `EAGAIN`。

Pipe 复制路径通常包含 user A → kernel pipe buffer → user B。确切实现和优化依赖内核，课程不把示意图误当每个 CPU 指令级实现。

## 4. ROS2 中如何映射

ROS2 不用一个匿名 pipe 实现通用 topic，原因包括：

- pipe 依赖 fd 继承，不能自然做动态 discovery；
- 默认单向且点对点，不提供 pub/sub fan-out；
- 没有 topic/type/QoS/history/durability；
- 不跨机器；
- 没有 DDS endpoint compatibility 与 distributed discovery。

但 pipe 教会的 stream framing、有限 buffer、blocking、EOF、fd lifecycle，会原样出现在更复杂 transport 的某些边界。

## 5. Linux 底层发生了什么

本实验故意使用两个 executable：

```text
pipe_process_a
  pipe2(O_CLOEXEC)
  fork()
  ├─ parent: close(read_fd), serialize + write_all, close(write_fd), waitpid
  └─ child : close(write_fd), clear FD_CLOEXEC on read_fd, exec(pipe_process_b)
                                                      │
                                                      └─ read_exact + decode
```

默认 `O_CLOEXEC` 是安全习惯，防止 fd 意外泄漏到 exec 后程序。实验只对需要传递给 `pipe_process_b` 的 read end 显式清除 `FD_CLOEXEC`。这个“默认关闭、按需授权”比直接创建永久可继承 fd 更接近真实工程。

`write_all/read_exact` 循环处理 `EINTR` 和 partial I/O。对 TCP 也复用同一思维。

## 6. 动手实验

### 实验猜想

1. child 的 PID 在 `exec` 前后是否变化？
2. A 一次写 32 bytes，B 一次 read 是否由 POSIX 保证返回 32？
3. A 发完消息但不关闭 write end，B 的下一次 read 会发生什么？
4. `pipe_process_b` 单独启动为什么失败？

### 运行

```bash
build/stage1/pipe_process_a
```

预期关键现象：

```text
pipe_process_a pid=... child_pid=...
pipe_process_b pid=... inherited_read_fd=...
pipe_rx seq=0 ... latency_us=...
...
pipe_process_b eof samples=5
pipe_process_a child_exit=0
```

每个样本使用显式 32-byte wire format。B 收到 EOF 是因为 A 和不再需要的 child write end 都被关闭。

## 7. 如何观察系统

```bash
strace -ff -ttT \
  -e trace=pipe2,clone,execve,fcntl,close,read,write,wait4 \
  build/stage1/pipe_process_a
```

观察：

- 哪个 PID 调用了 `execve`；
- read fd 如何跨 exec 保留；
- stdout 本身也是 `write`，不要与 pipe write 混淆，按 fd 判断；
- 最后一次 `read` 返回 0；
- `close` 的顺序如何决定 EOF。

运行 `lsof -p PID` 时程序可能结束太快。可增大 count/interval，或在 debugger 中暂停；匿名 pipe 在 lsof 中通常显示 `FIFO/pipe:[inode]`，而不是一个可抓包的网络连接。

## 8. 常见坑

- 假定一写一读；
- 忽略 `EINTR` 和 partial I/O；
- 父进程忘记关闭 read end，或 child 继承多余 write end；
- 不处理 `SIGPIPE/EPIPE`（reader 提前退出时）；
- 用 native struct 当协议，忽略 padding/endianness/version；
- 把 buffer capacity 当 QoS depth；
- 用 tcpdump 观察 pipe：它不经过 IP 网络栈。

本实验忽略 `SIGPIPE` 并将 `EPIPE` 转成可读异常，避免进程在打印诊断前被信号直接终止。

## 9. 真实机器人案例

机器人进程 supervisor 可通过 child stdout/stderr pipe 收集日志，或向工具子进程输送数据。它适合固定亲缘关系，不适合需要动态端点、跨主机、QoS 和 N 对 M 的 sensor bus。

**【求职价值】** Partial I/O、close-on-exec、EOF 和 fd 泄漏是 Linux C++ 高频追问，也是排查 bridge/driver 子进程挂死的直接能力。

## 10. 面试题

1. pipe 为什么不是消息队列？
2. 什么条件下 read 返回 EOF？
3. `PIPE_BUF` 原子性保证和完整 message read 有什么区别？
4. pipe 满时会发生什么？
5. 为什么 ROS2 topic 不直接用 pipe？

## 11. 🔎 Source Dive 与本章总结

阅读 `man 2 pipe`、`man 7 pipe`、`man 2 read`、`man 2 write`、`man 2 fcntl`、`man 2 execve`。先用 strace 验证语义，再决定是否阅读 Linux `fs/pipe.c`。

> Pipe 是有限容量、由内核托管、靠 fd lifecycle 表达对端生存状态的字节流。framing 和完整读写是应用协议责任。

第一遍可以把它说成：

> Pipe 是一根容量有限的字节管。发送方要负责写完整，接收方要自己分清消息；所有写入端都关掉后，接收方才能知道传输结束。
