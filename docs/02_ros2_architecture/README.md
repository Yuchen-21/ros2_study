# Stage 02 — ROS2 分层与 publish 生命周期（规划）

重要度：⭐⭐⭐　预计：8～12 小时　依赖：Stage 01

## 核心问题

执行 `publisher_->publish(msg)` 后，哪一层处理 C++ 类型，哪一层提供 C API 边界，哪一层适配 vendor，哪一层实现 DDS 语义？返回时又能保证什么？

## 学习目标

- 画出 Application → rclcpp → rcl → rmw → DDS → RTPS → transport → kernel；
- 解释 rclcpp/rcl/rmw 的边界，不把它们统称为“DDS wrapper”；
- 识别 ROSIDL 生成的 C/C++ typesupport；
- 沿 Humble 代码追到 `rcl_publish`、`rmw_publish` 和 vendor DataWriter；
- 分清 typed、serialized、intra-process publish 路径。

## 实验列表

| ID | 实验 | 主要证据 |
|---|---|---|
| A02-01 | 最小 stamped publisher/subscriber | 应用时间戳与 PID/TID |
| A02-02 | Fast DDS/Cyclone DDS 切换 | `RMW_IMPLEMENTATION`、加载库、行为对照 |
| A02-03 | 动态库观察 | `ldd`、`LD_DEBUG=libs`、`/proc/PID/maps` |
| A02-04 | GDB 分层断点 | `rcl_publish`、`rmw_publish` backtrace |
| A02-05 | typed vs serialized publish | 调用路径、CPU 与类型支持 |

## 🔎 Source Dive

从 `rclcpp/include/rclcpp/publisher.hpp` 的 `Publisher<T>::publish` 开始，只追下一层入口；随后阅读 `rcl/src/rcl/publisher.c` 的 `rcl_publish` 与当前 RMW 的 `rmw_publish`。不在第一次阅读中展开整个 Fast DDS。

## 【求职价值】

RMW vendor 切换、崩溃堆栈和性能问题都会跨仓库。能指出“问题属于哪一层”比背 API 更能体现中间件工程能力。

## 完成定义

- [ ] 能用 2 分钟解释 publish 调用链与每层职责。
- [ ] 能用 debugger 捕获至少三层函数。
- [ ] 能声明 Humble + 具体 RMW 前提，而不把实现细节说成 ROS2 规范。
