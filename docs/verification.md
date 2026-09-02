# Verification Record

验证日期：2026-09-02

## Target environment

| Item | Verified value |
|---|---|
| Container OS | Ubuntu 22.04.5 LTS |
| ROS | ROS2 Humble |
| Architecture | arm64 |
| Compiler | GCC 11.4.0 |
| CMake | 3.22.1 |
| Build | standalone CMake and ament_cmake/colcon |

## Results

### ROS2/colcon path

```bash
source /opt/ros/humble/setup.bash
colcon build \
  --base-paths labs/01_linux_ipc \
  --packages-select linux_ipc_lab \
  --symlink-install \
  --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo
ctest --test-dir build/linux_ipc_lab --output-on-failure
```

Result: 1 package built; 4/4 CTest passed；安装空间 source 后，`ros2 run linux_ipc_lab thread_shared_state --iterations 1000` 正常运行并满足 counter invariant。

### Documented one-command path

源码以只读 bind mount 提供，build tree 位于容器 `/tmp`：

```bash
STAGE1_BUILD_DIR=/tmp/ros2_comm_lab_stage1_build \
  ./scripts/build_stage1.sh
```

Result: all 12 executables built; 4/4 CTest passed.

### Repetition

`run_ipc_pairs.sh` 在同一 Humble 容器连续运行 20 轮，UDS/TCP/UDP/POSIX SHM 全部通过。此重复测试用于发现启动/初始化竞态，不是性能 benchmark。

### Sanitizers

同一目标容器以 Debug、`-fsanitize=address,undefined` 和 frame pointers 重新构建，设置 ASan/UBSan `halt_on_error=1` 后运行全部 4 个测试。结果：4/4 通过，未报告 sanitizer 错误。

## What the four tests prove

| Test | Assertion |
|---|---|
| `thread_shared_state_test` | 两 worker 的 mutex-protected counter 满足最终 invariant |
| `process_memory_isolation_test` | child 写后 parent 的 process-local value 保持 42 |
| `pipe_process_test` | fd 跨 exec、三条 frame 完整解码、close 后 EOF、child 正常退出 |
| `ipc_pairs_integration_test` | UDS/TCP/UDP 各接收五条且无 gap；SHM 完成 ready/publish/ack/shutdown/unlink |

## What these tests do not prove

- 不证明任一 IPC 是“最快”；
- 不覆盖真实 NIC、packet loss、MTU 或 RTPS；
- 不覆盖多 writer/multi-reader SHM；
- 不证明跨机器 timestamp 可直接相减；
- 不替代 sanitizer、长稳、网络故障注入和 ROS2 tracing；
- 不对微秒 latency 设置 CI 阈值。

这些内容分别属于 Stage 03、05～09 和 Capstone。
