# message_center

<p align='right'>士继 DREAMER 战队</p>
<p align='right'>作者：Xun Wei</p>
<p align='right'>参考文献：湖南大学跃鹿战队相关开源代码与文档</p>

## 说明

本目录是 **RoboMaster 无人机云台部分代码** 中的消息中心模块目录。

当前目录只包含两个需要说明的源码文件：

- `message_center.h`
- `message_center.c`

该模块的作用不是提供通用事件队列，而是为当前工程中的应用层提供“只保留最新值”的话题通信能力。它服务于 `robot_cmd`、`gimbal` 和 `shoot` 之间的命令与反馈交换，是当前应用层解耦的基础设施。

## 在工程中的位置

该模块在当前工程中的调用关系如下：

- 在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中，由 `RobotInit()` 调用 `MessageCenterInit()`
- 在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中，在各应用初始化完成后调用 `MessageCenterFreezeRegistry()`
- 在 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 中注册并使用 `gimbal_cmd`、`gimbal_feed`、`shoot_cmd`、`shoot_feed`
- 在 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 中订阅 `gimbal_cmd`，发布 `gimbal_feed`
- 在 [shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c) 中订阅 `shoot_cmd`，发布 `shoot_feed`

因此，当前工程中的应用模块并不直接互相调用，而是通过本模块传递消息。

## 当前目录文件

### `message_center.h`

该文件定义消息中心的对外数据结构和对外接口。

主要内容包括：

- 话题数量、订阅者数量、话题名长度和最大消息长度等资源上限
- `Publisher_t` 话题对象定义
- `Subscriber_t` 订阅者对象定义
- 话题统计结构体和订阅者统计结构体
- 消息中心初始化、冻结、注册、发布、读取和统计接口

从当前实现看，`Publisher_t` 实际上代表的是“话题对象”，而不是传统意义上只负责发送的发布者句柄。每个话题对象内部保存一份最新消息、当前版本号、订阅者链表和统计信息。

### `message_center.c`

该文件实现消息中心的内部逻辑。

主要负责以下工作：

1. 维护静态话题池和静态订阅者池
2. 校验话题名称和消息长度
3. 注册话题和注册订阅者
4. 发布最新消息
5. 让订阅者读取尚未读取的最新消息
6. 导出运行时统计信息
7. 通过临界区保护共享状态

该实现不使用动态内存，也不保存历史消息队列，而是采用静态池加版本号的方式管理消息。

## 核心语义

当前这版消息中心的核心语义是：

- 每个话题只保存一份最新消息
- 发布新消息时直接覆盖旧消息
- 订阅者通过版本号判断自己是否读到了新消息
- 不维护 FIFO 历史队列

这意味着它更接近“最新值邮箱”，而不是“事件队列”。

这种语义适合当前工程中的控制类数据，例如：

- 云台目标角度
- 云台反馈姿态
- 发射模式
- 拨弹电机反馈

这些数据更关心“当前最新状态”，不需要逐条回放历史命令。

## 当前工程中的话题拓扑

当前工程实际只使用 4 个话题：

- `gimbal_cmd`
- `gimbal_feed`
- `shoot_cmd`
- `shoot_feed`

具体关系如下：

### 在 `robot_cmd` 中

- 发布 `gimbal_cmd`
- 订阅 `gimbal_feed`
- 发布 `shoot_cmd`
- 订阅 `shoot_feed`

### 在 `gimbal` 中

- 订阅 `gimbal_cmd`
- 发布 `gimbal_feed`

### 在 `shoot` 中

- 订阅 `shoot_cmd`
- 发布 `shoot_feed`

这 4 个话题构成了当前工程完整的应用层消息拓扑。

## 对外接口说明

当前最常用的接口包括：

- `MessageCenterInit()`
- `MessageCenterFreezeRegistry()`
- `PubRegister()`
- `SubRegister()`
- `PubPushMessage()`
- `SubGetMessage()`

它们的使用方式如下：

### 初始化阶段

在系统初始化阶段：

1. 调用 `MessageCenterInit()`
2. 由各应用调用 `PubRegister()` 和 `SubRegister()`
3. 调用 `MessageCenterFreezeRegistry()`

这样做的目的是把消息中心分成两个阶段：

- 初始化阶段，只允许建立通信拓扑
- 运行阶段，只允许收发消息

### 运行阶段

应用层在运行阶段按以下方式使用消息中心：

- 发布者通过 `PubPushMessage()` 写入某个话题的最新数据
- 订阅者通过 `SubGetMessage()` 获取自己尚未读取的最新版本

如果某个订阅者在两次读取之间错过了中间版本，模块不会补发旧消息，而是只记录漏读统计。

## 辅助接口

除了基本注册和收发接口外，当前模块还提供：

- `MessageCenterRegistryFrozen()`
- `SubPeekMessage()`
- `SubHasNewMessage()`
- `PubGetTopicStats()`
- `SubGetSubscriberStats()`
- `MessageCenterTopicCount()`
- `MessageCenterSubscriberCount()`

这些接口主要用于：

- 查询当前消息中心状态
- 在不消费消息的前提下观察最新值
- 判断是否存在新消息
- 调试话题发布频率、消息覆盖次数和订阅者漏读情况

## 内部设计要点

### 静态池

消息中心内部使用静态话题池和静态订阅者池，而不是动态分配。

这样设计的原因是：

- MCU 工程中不希望基础设施依赖堆内存
- 可以避免堆碎片和运行期分配失败
- 资源上限清晰，便于控制系统规模

### 版本号

每个话题内部维护一个版本号，每次成功发布都会递增。

订阅者内部只记录：

- 自己订阅哪个话题
- 自己上次读到了哪个版本

这样就不需要给每个订阅者准备独立消息队列，也能判断是否有新消息。

### 覆盖统计

如果旧版本尚未被所有相关订阅者读取，就被新版本直接覆盖，则会增加 `overwrite_count`。

这个统计的目的不是补发历史消息，而是帮助开发者判断：

- 发布频率是否过高
- 订阅任务是否处理过慢

### 冻结注册表

消息中心支持在初始化结束后冻结注册表。

冻结后不再允许注册新话题或新订阅者，这样能避免运行期动态修改通信拓扑，提高系统结构稳定性。

### 临界区保护

由于当前工程中的 `robot_cmd`、`gimbal`、`shoot` 和其他任务运行在 FreeRTOS 环境下，消息中心内部通过临界区保护共享状态，避免并发读写造成状态不一致。

## 当前目录职责

`message_center` 在当前工程中的职责是：

- 提供应用层之间的消息解耦机制
- 提供“仅保留最新值”的状态同步能力
- 提供消息收发与统计接口
- 作为云台与发射控制链路中的基础设施模块存在

它不负责业务逻辑，也不负责底层外设驱动。

## 维护约束

维护本目录时，建议遵守以下约束：

- 保持当前“只保留最新值”的消息语义
- 不要把该模块重新改回历史队列模型
- 新增话题时同步检查资源上限是否足够
- 应用层消息结构体统一从 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h) 获取
- 注册行为应继续限制在初始化阶段完成

## 备注

本说明文件只覆盖当前目录中的 `.c/.h` 文件。

文中提到 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c)、[robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c)、[gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c)、[shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c) 仅用于说明本模块的调用位置和使用方式，不替代这些目录自己的说明文件。
