# Stage 10 — ROS2 Communication Troubleshooting Playbook（规划）

重要度：⭐⭐⭐　预计：12～18 小时　依赖：Stage 03～09

## 排查主干

```text
Application
  ↓ 是否真的 publish？sequence/stamp 是否前进？
Executor
  ↓ entity ready？线程忙？callback group/锁？
RMW
  ↓ 实现一致？type support？事件？
DDS
  ↓ discovered？matched？QoS/history/resource？
Transport
  ↓ UDP 还是 SHM？send/receive queue？
Network
  ↓ route/interface/multicast/firewall/MTU/loss？
OS
  ↓ CPU/memory/fd/socket buffer/scheduler/time sync？
```

## 十个故障实验

1. publisher 正常调用，subscriber 不 callback；
2. topic list 可见，echo 无数据；
3. publisher 100 Hz，subscriber 40 Hz；
4. CPU 100%；
5. callback 卡死；
6. Wi-Fi 模拟链路下 DDS 不稳；
7. Domain ID 不一致；
8. QoS incompatible；
9. PointCloud 导致系统延迟；
10. 慢 callback 阻塞无关 topic。

每个案例都包含：症状、三条候选假设、最小侵入采证、定位树、修复、回归测试和“容易误判的证据”。

## 工具产出

`collect_ros2_comm_diagnostics.sh` 将采集环境/RMW、node/topic info、进程线程、socket、route、DDS 配置摘要、CPU/RSS 和可选短 trace。脚本不自动修改系统。

## 【求职价值】

高级面试通常给模糊症状。高质量回答会先定义观测口径，再按层缩小故障域，并在每一步说明“这个证据能排除什么、不能排除什么”。

## 完成定义

- [ ] 每个案例在未知故障注入参数时也能定位。
- [ ] 不使用“重装/重启”作为首个步骤。
- [ ] 修复后有同条件回归和反例验证。
