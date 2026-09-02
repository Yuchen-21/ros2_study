# 贡献与实验记录约定

这个仓库既是课程，也是可重复实验平台。新增内容应满足以下约定。

## 代码

- 使用 C++17；资源必须由 RAII 对象持有。
- 检查系统调用返回值，并在错误中保留 `errno` 语义。
- 网络字节流必须定义 framing；禁止依赖“一次 send 对应一次 recv”。
- 性能实验使用 `steady_clock`/`CLOCK_MONOTONIC`，墙上时间只用于日志关联。
- 默认测试不能要求 root、外网或图形界面；需要 capability/root 的故障注入单独标记。

## 文档

实验必须包含：猜想、命令、命令目的、预期现象、可证伪判断、常见异常和清理方式。
性能结论必须记录 CPU、内核、编译类型、消息大小、频率、RMW、QoS 和传输配置。

## Definition of Done

```bash
./scripts/build_stage1.sh
./scripts/run_stage1_smoke.sh
git diff --check
```

后续 ROS2 阶段还需要在 Ubuntu 22.04 + ROS2 Humble 容器中执行对应 colcon、launch test 和 tracing smoke test。
