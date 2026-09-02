# Stage 05 — DDS QoS、History 与背压（规划）

重要度：⭐⭐⭐　预计：12～18 小时　依赖：Stage 03、04

## 核心问题

RELIABLE 为什么不等于“永不丢、永远更好”？Depth 是哪一层的深度？慢订阅者造成的是丢新、丢旧、阻塞还是延迟累积？

## 学习目标

- 从 writer/reader history cache 与 resource limits 解释 history/depth；
- 从 ACKNACK/重传解释 reliability；
- 区分 durability、deadline、lifespan、liveliness；
- 判断 requested/offered compatibility；
- 为 camera、lidar、control、map、configuration 制定 QoS。

## 实验列表

| ID | 实验 | 指标 |
|---|---|---|
| A05-01 | 1000 Hz + 慢订阅者 | sequence gap、message age |
| A05-02 | depth 1/10/100 | P50/P99 age、RSS |
| A05-03 | BEST_EFFORT vs RELIABLE | loss/delay/jitter 下吞吐与尾延迟 |
| A05-04 | VOLATILE vs TRANSIENT_LOCAL | late joiner |
| A05-05 | incompatible QoS | event/log/endpoint info |
| A05-06 | deadline/lifespan/liveliness | status callback 与过期样本 |

网络故障用 `tc netem`，并记录 qdisc、网卡、RMW、频率和消息大小。`ros2 topic hz` 只是一个额外订阅者，可能改变可靠通信的行为，不能当无扰动真值。

## 实验猜想

控制命令在 20% 丢包、100 ms 重传延迟下使用 RELIABLE，旧命令最终到达是否一定比丢弃旧命令安全？用 freshness deadline 回答。

## 【求职价值】

QoS 是业务时序需求到 DDS 行为的翻译层。优秀答案会同时谈 freshness、资源上限、慢消费者、网络和安全状态。

## 完成定义

- [ ] 能从抓包解释 reliable 控制流量。
- [ ] 能用 message age 而非只用 receive Hz 分析 backlog。
- [ ] 能为五类机器人数据写出并捍卫 QoS 决策。
