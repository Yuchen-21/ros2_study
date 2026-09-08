# ROS 2 使用基础：先让配送站的几个岗位说上话

位置：[主线](../00_course_map.md)顺序 1～2。前一站：[IPC](../01_linux_ipc/README.md)；下一站：[状态机](../16_state_machine/README.md)。目录 15 是资料编号。

状态：导学和官方示例入口已提供；本仓自有 ROS 2 基础包、慢回调与 QoS 故障注入尚待实现。基线为 Humble，命令需安装对应示例包。

## 第一遍读法与先记三件事

第一次只学习下面五课。先不用 GDB 追 `rmw_publish`，也不用背发现报文。

1. 节点像配送站里的岗位，通信接口像岗位之间传话的约定。
2. 电量这种持续更新的信息、一次查询、耗时配送，应该使用不同接口。
3. 收到消息、执行回调、完成任务是不同时间点。

类比边界：节点是逻辑对象，一个进程可以容纳多个节点；topic 不是电话线，也不对应唯一 socket。

## 五课顺序

| 课 | 配送场景 | 学什么 | 动手与过关条件 |
|---:|---|---|---|
| 1 | 电池每秒报告电量 | node、topic、message、publisher/subscription | 跑 talker/listener，用 `node info`、`topic info -v` 找发送方、接收方、类型 |
| 2 | 查询库存、设置配送速度 | service、parameter | 跑加法服务；解释一次请求响应与节点配置的区别 |
| 3 | 下单后持续反馈进度 | action、goal、feedback、result、cancel；launch | 跑 Fibonacci action；看懂目标、反馈和结果，沿官方 action 编写教程补做取消；用官方 launch 教程一起启动多个节点 |
| 4 | 接单员等配送结果时，还要接取消通知 | spin、Executor、callback、异步请求 | 画出“接单→提交目标→回调返回→接收结果”；解释阻塞等待为什么可能卡住结果回调 |
| 5 | 电量已过期、接收端漏消息 | history/depth、reliability、durability、数据时效 | 用 `topic info -v` 记录两端 QoS；分清队列深度与消息时效检查 |

前三课使用 [Humble 初学者 CLI 教程](https://docs.ros.org/en/humble/Tutorials/Beginner-CLI-Tools.html)和 [Humble 客户端库教程](https://docs.ros.org/en/humble/Tutorials/Beginner-Client-Libraries.html)。其中 Action 的完整编写练习见 [Humble C++ Action 教程](https://docs.ros.org/en/humble/Tutorials/Intermediate/Writing-an-Action-Server-Client/Cpp.html)，Launch 见 [Humble Launch 教程](https://docs.ros.org/en/humble/Tutorials/Intermediate/Launch/Launch-Main.html)。网页受访问限制时可查 [ros2_documentation 的 humble 源文档](https://github.com/ros2/ros2_documentation/tree/humble/source/Tutorials)。这些是外部练习，不代表仓库已实现对应实验包。

## 第一组实验：三个传话方式

以下命令在仓库根目录执行；官方示例实际上不依赖当前目录。每个新终端先运行：

```bash
source /opt/ros/humble/setup.bash
ros2 pkg executables demo_nodes_cpp
ros2 pkg executables action_tutorials_cpp
```

目的是确认环境和示例存在。若不存在 Humble setup 文件，先准备 Humble 环境；不要用 ROS 1 Noetic 替代。若包不存在，按对应官方教程安装/构建后再做，不必重建 Stage 01。

先猜：listener 晚启动，是否一定补收所有旧数据？然后分别在两个终端运行：

```bash
# 终端 A：持续发送
ros2 run demo_nodes_cpp talker
```

```bash
# 终端 B：持续接收
ros2 run demo_nodes_cpp listener
```

终端 C 观察实际通信实体；成功标志是 `/chatter` 两侧存在且 B 持续收到消息。内容编号与发现耗时允许变化，不能凭“node list 有名字”断定通信正常。

```bash
ros2 node list
ros2 topic info /chatter -v
```

第二组先用 `Ctrl+C` 停止 A、B，再在 A 启动服务，在 B 发请求：

```bash
# 终端 A
ros2 run demo_nodes_cpp add_two_ints_server
```

```bash
# 终端 B：预期响应 sum 为 5
ros2 service call /add_two_ints example_interfaces/srv/AddTwoInts "{a: 2, b: 3}"
```

第三组停止服务，在 A 启动耗时任务，在 B 观察反馈：

```bash
# 终端 A
ros2 run action_tutorials_cpp fibonacci_action_server
```

```bash
# 终端 B
ros2 action send_goal /fibonacci action_tutorials_interfaces/action/Fibonacci "{order: 5}" --feedback
```

成功标志是 goal 被接受、出现 feedback、最终出现 result。这个例子计算数列；“配送”是后续替换进去的业务，不是这里已经实现的功能。停止 CLI 进程不能作为服务端已经取消任务的证据；取消要由客户端请求，并观察服务端终态。

**可证伪判断**：如果服务端还没启动，客户端不能正常得到响应；如果把 action 类型写错，也不能因为名字相同就成功执行。修改一个变量再重试，保存错误和修正后的输出。

**常见异常与清理**：等待服务/服务器时先检查同一 Humble 环境、名称、类型和 Domain；不要盲目重启。所有示例用各终端 `Ctrl+C` 结束，不操作设备、不持久修改配置。

## 从“能通信”走向“能组织任务”

接下来的机器人采用这组教学约定：

| 数据或意图 | 接口选择 | 为什么 |
|---|---|---|
| 电量、位置、任务状态 | Topic | 不断更新，可能有多个观察者 |
| 单次状态查询 | Service | 请求与响应短小 |
| 低电量阈值 | Parameter | 属于节点配置，修改需校验 |
| 导航/配送 | Action | 需要反馈、结果和取消 |
| 节点启动及参数组合 | Launch | 描述进程如何启动；就绪仍需状态和响应确认 |

回调好比柜台每次处理一张单据。耗时任务交出去后先返回，让 Executor 还能处理电量、取消和结果回调。多线程并不自动消除同一 callback group 内的互斥，也不自动保护共享状态。顺序 2 先能解释这两句话，深入实验留到[04](../04_executor_waitset/README.md)。

QoS 好比各邮箱的保留与投递规则。但 `depth=1` 不会让十秒前的电量自动变成新数据；任务层还要检查样本的时间戳或接收时刻。完整兼容矩阵和网络实验留到[05](../05_qos/README.md)。语义参考 [Humble QoS 官方说明](https://docs.ros.org/en/humble/Concepts/Intermediate/About-Quality-of-Service-Settings.html)。

## 完成检查

- [ ] 能为电量、参数、短查询、导航选择接口并解释原因。
- [ ] 能指出 goal 接受、任务成功和取消完成是三个不同事实。
- [ ] 能解释为什么不能在执行结果回调所需的线程里一直等结果。
- [ ] 知道 topic 名称可见不证明 QoS、类型和数据路径都正常。

做到后进入[状态机](../16_state_machine/README.md)。后续补写自有 ROS 示例时，优先覆盖取消确认、慢回调与旧电量反例。
