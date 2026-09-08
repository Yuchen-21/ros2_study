# Stage 12 — ROS2 Humble 源码阅读路线（规划）

重要度：⭐⭐　预计：12～20 小时　依赖：Stage 02、04、06

> **白话预告**：源码很多时，不从第一页硬读，而是跟着“一条消息”或“一次等待”跨过各层。本阶段训练的是带着问题找入口和验证调用链。当前只有规划；术语见[术语表](../00_glossary.md)。

## 阅读纪律

一次只追一个问题；到抽象边界就先停下来写结论。模板实例、虚函数和 vendor callback 容易让静态搜索断链，因此配合 GDB backtrace、compile database 和动态库 maps。

## 路线 A：publish

```text
demo_nodes_cpp talker
  → rclcpp::Publisher<T>::publish
  → do_inter_process_publish
  → rcl_publish
  → rmw_publish
  → rmw_fastrtps_cpp / rmw_cyclonedds_cpp
  → DDS DataWriter
  → vendor serialization / transport
```

## 路线 B：spin

```text
Executor::spin
  → get_next_executable / wait_for_work
  → rcl_wait
  → rmw_wait
  → ready entity
  → execute_subscription/timer/service
  → AnySubscriptionCallback
```

## 仓库与阅读目的

| Repository | 目录/对象 | 目的 |
|---|---|---|
| demos | `demo_nodes_cpp` | 找到最小调用者 |
| rclcpp | publisher/executor/subscription | C++ policy、ownership、dispatch |
| rcl | publisher/wait/subscription C API | C 抽象边界与错误处理 |
| rmw | API declarations | vendor-neutral contract |
| rmw_fastrtps | cpp/shared_cpp | ROS type support 到 Fast DDS |
| Fast DDS/Fast CDR | DataWriter/RTPS/CDR | vendor 行为 |
| rosidl | generators/typesupport | message 到 serialization glue |

正文将使用 Humble branch/tag 与工作区实际 apt 版本对应，不用 Rolling 文件行号冒充 Humble。

## 实验列表

- `rg` 建入口、调用图；
- GDB pending breakpoint 跨 shared library；
- 对比两个 RMW 的分叉点；
- 读 tracepoint provider；
- 每层制作“一句话职责 + 输入输出 + ownership”卡片。

## 【求职价值】

真正的源码能力是快速定位责任层、构建可验证假设，并说明版本差异；不是背函数行号。

## 完成定义

- [ ] 两条路线都有静态链接和动态 backtrace 证据。
- [ ] 能指出哪些函数是 API contract，哪些是 vendor implementation。
- [ ] 能在源码变更后用符号而非行号重新定位。
