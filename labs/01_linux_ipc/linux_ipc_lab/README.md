# linux_ipc_lab

一个面向系统观察而不是 API 演示的 C++17 包。所有程序检查系统调用、使用 RAII 关闭资源，并为长时间阻塞设置可配置 deadline。

## 1. 构建

仓库根目录：

```bash
./scripts/build_stage1.sh
```

或普通 CMake：

```bash
cmake -S labs/01_linux_ipc/linux_ipc_lab -B build/stage1 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/stage1 --parallel
ctest --test-dir build/stage1 --output-on-failure
```

`build_stage1.sh` 支持用 `STAGE1_BUILD_DIR=/absolute/path` 覆盖构建目录。容器 bind mount 改变源码绝对路径时应使用独立 build tree，不能复用宿主机的 `CMakeCache.txt`。

ROS2 Humble：

```bash
source /opt/ros/humble/setup.bash
colcon build --base-paths labs/01_linux_ipc \
  --packages-select linux_ipc_lab \
  --symlink-install
colcon test --packages-select linux_ipc_lab
colcon test-result --verbose
source install/setup.bash
ros2 run linux_ipc_lab thread_shared_state
```

代码不调用 ROS2 API；`ament_cmake` 只负责把它作为课程 workspace 中的标准包构建/安装。

## 2. Executable 与参数

| Executable | 参数 | 默认值 |
|---|---|---|
| `thread_shared_state` | `--iterations`、`--hold-ms` | 100000、0 |
| `process_memory_isolation` | `--hold-ms` | 0 |
| `pipe_process_a` | `--count`、`--interval-ms`、`--child` | 5、20、同目录 B |
| `pipe_process_b` | `--fd` | 必须由 A 传入 |
| `unix_domain_subscriber` | `--count`、`--path`、`--timeout-ms` | 10、UID 专用路径、5000 |
| `unix_domain_publisher` | `--count`、`--path`、`--interval-ms` | 10、UID 专用路径、20 |
| `tcp_subscriber` | `--count`、`--port`、`--timeout-ms` | 10、39001、5000 |
| `tcp_publisher` | `--count`、`--port`、`--interval-ms`、`--connect-timeout-ms` | 10、39001、20、5000 |
| `udp_subscriber` | `--count`、`--port`、`--timeout-ms` | 10、39002、5000 |
| `udp_publisher` | `--count`、`--port`、`--interval-ms` | 10、39002、20 |
| `shared_memory_writer` | `--name`、`--count`、`--interval-ms`、`--timeout-ms` | UID 专用名、10、20、5000 |
| `shared_memory_reader` | `--name`、`--count`、`--timeout-ms` | UID 专用名、10、5000 |

参数解析故意拒绝未知/重复项，让命令拼写错误立即失败，而不是悄悄跑了另一个实验条件。

## 3. 协议

Pipe、UDS、TCP 和 UDP 使用固定 32-byte wire record：

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | magic `RIPC` |
| 4 | 2 | protocol version |
| 6 | 2 | reserved flags |
| 8 | 4 | sequence |
| 12 | 8 | `CLOCK_MONOTONIC` timestamp ns |
| 20 | 4 | x float bits |
| 24 | 4 | y float bits |
| 28 | 4 | z float bits |

整数和 float bit pattern 均以 big-endian 编码。实验没有直接发送 C++ struct，原因是 native layout 包含 ABI/padding/endianness 风险。这个手写协议用于建立直觉；它不是 CDR 的替代品，Stage 06 会对照 ROSIDL/CDR。

## 4. Shared-memory protocol

Writer 拥有 shared-memory object，reader 与 writer 经 process-shared robust mutex 和 condition variable 握手：


```text
create → initialize under flock → reader_ready
   → publish generation N → reader copies/acknowledges N
   → publish N+1 ...
   → final ack → shutdown → shm_unlink
```

只有一个 slot，因此每次 publication 必须等 reader acknowledgement。这使 ownership 明确、资源有界，也有意展示 slow consumer 如何把 backpressure 传回 writer。高吞吐 DDS SHM 会使用多 slot/ring/pool，但仍逃不开 ownership 状态机。

## 5. 自动化验证

```bash
./scripts/run_stage1_smoke.sh
```

Smoke test 检查：

- mutex-protected thread invariant；
- fork/COW 后 parent invariant；
- pipe child 通过 exec 收到全部样本并看到 EOF；
- UDS/TCP/UDP 各收 5 条且 sequence gap 为 0；
- SHM reader acknowledgement 和 owner unlink；
- client/server 超时防止 CI 永久挂起。

测试只绑定 loopback 和临时 UDS/SHM 名称，不要求 root 或外部网络。

## 6. 设计限制

- 单 publisher/single subscriber，便于先验证语义；
- wire sequence 是 uint32，长时间运行的 wrap-around 未在 Stage 01 展开；
- 单向 latency 只在同机同一 monotonic clock domain 有意义；
- UDP baseline payload 很小，不代表 RTPS fragmentation 行为；
- SHM 直接共享 ABI layout，只允许同主机同版本程序；
- signal 强杀可能留下 SHM object；文档给出安全确认与清理流程。
