# robot_cmd

<p align='right'>士继 DREAMER 战队</p>
<p align='right'>作者：Xun Wei</p>
<p align='right'>参考文献：湖南大学跃鹿战队相关开源代码与文档</p>

## 说明

本目录是 **RoboMaster 无人机云台部分代码** 中的控制指令应用目录。  
本说明文件只以当前目录中的 `.c` 与 `.h` 文件为核心进行说明，但会结合其上下游调用关系，说明这些文件在整套控制链中的作用和使用方式。

当前目录内需要说明的文件如下：

- `robot_cmd.c`
- `robot_cmd.h`

## 在工程中的位置

`robot_cmd` 是当前应用层的控制中枢。

它在工程中的上下游关系如下：

- 在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中由 `RobotInit()` 调用 `RobotCMDInit()` 完成初始化
- 在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中由 `RobotTask()` 周期性调用 `RobotCMDTask()`
- 命令与反馈数据结构定义在 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h)
- 通过消息中心向 `gimbal` 应用发布 `gimbal_cmd`
- 通过消息中心订阅 `gimbal` 应用发布的 `gimbal_feed`
- 通过消息中心向 `shoot` 应用发布 `shoot_cmd`
- 通过消息中心订阅 `shoot` 应用发布的 `shoot_feed`

因此，`robot_cmd` 的核心职责不是直接控制执行机构，而是把输入设备、系统状态和下游应用连接起来。

## 文件关系

### `robot_cmd.h`

该文件对外声明 `robot_cmd` 应用的两个入口函数：

- `RobotCMDInit()`
- `RobotCMDTask()`

它的作用是向应用层总入口暴露本目录的初始化接口和周期任务接口，使 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 可以统一调度 `robot_cmd` 应用，而不需要关心内部实现细节。

### `robot_cmd.c`

该文件实现当前工程的控制输入解析、模式切换、控制量生成和消息发布逻辑。

从整体职责上看，它完成以下工作：

1. 获取控制输入
2. 获取下游应用反馈
3. 根据系统状态生成控制命令
4. 将控制命令发布给云台和发射机构

这里的“控制输入”当前主要来自遥控器和键鼠映射。  
这里的“下游应用反馈”当前来自：

- `gimbal_feed`
- `shoot_feed`

最终生成并发布的控制命令为：

- `gimbal_cmd`
- `shoot_cmd`

## 初始化逻辑

`RobotCMDInit()` 的工作分为两部分。

### 1. 输入接口初始化

当前通过：

- `RemoteControlInit(&huart3)`

初始化遥控器输入，并保存返回的遥控器数据指针。

这意味着 `robot_cmd` 是当前工程中遥控器数据进入应用层的主要入口。

### 2. 消息中心注册

当前代码中，`robot_cmd` 在消息中心中承担“两发两收”的角色：

- 发布 `gimbal_cmd`
- 订阅 `gimbal_feed`
- 发布 `shoot_cmd`
- 订阅 `shoot_feed`

对应的数据类型都来自 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h)：

- `Gimbal_Ctrl_Cmd_s`
- `Gimbal_Upload_Data_s`
- `Shoot_Ctrl_Cmd_s`
- `Shoot_Upload_Data_s`

在当前工程中，消息中心已经采用“仅保留最新值”的语义，因此 `robot_cmd` 这里每次任务运行时只需要读取当前最新反馈，再生成当前最新控制命令即可。

## 主要内部逻辑

### `ModeSwitch()`

该函数根据遥控器拨杆状态切换系统控制模式。

当前系统状态包括：

- `SYS_ESTOP`
- `SYS_SAFE_STANDBY`
- `SYS_RC_CONTROL`
- `SYS_PC_CONTROL`

这些状态同样定义在 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h) 中。

设计上先把拨杆组合转换成统一的系统状态，再由后续控制逻辑根据系统状态工作，能够避免在多个控制分支中重复解析输入。

### `RemoteControl()`

该函数负责遥控器控制模式下的控制量生成。

当前主要完成：

- 根据拨杆位置选择云台控制模式
- 根据摇杆输入更新 yaw / pitch 目标
- 控制摩擦轮状态
- 控制拨弹模式
- 执行卡弹检测与反转逻辑
- 对云台角度目标做软限位约束

这里生成的控制命令不会直接写入电机，而是写入：

- `gimbal_cmd_send`
- `shoot_cmd_send`

随后由 `RobotCMDTask()` 统一发布。

### `MouseKeyControl()`

该函数负责键鼠控制模式下的控制量生成。

当前主要完成：

- 根据按键切换云台模式
- 根据鼠标位移生成云台角度增量
- 根据按键控制摩擦轮开关
- 根据按键与鼠标左键控制发射方式
- 复用卡弹检测与反转逻辑
- 对云台角度目标做软限位约束

虽然输入来源与 `RemoteControl()` 不同，但最终输出的命令结构完全一致，因此下游 `gimbal` 和 `shoot` 不需要区分命令来源。

## 周期任务逻辑

`RobotCMDTask()` 是 `robot_cmd` 应用的核心周期任务。

当前执行流程如下：

1. 先从消息中心读取：
   - `shoot_feed`
   - `gimbal_feed`
2. 调用 `ModeSwitch()` 判断当前系统状态
3. 根据系统状态选择不同控制分支：
   - 急停 / 安全待机
   - 遥控器控制
   - 键鼠控制
4. 对发射机构执行最终安全约束  
   当摩擦轮关闭时，强制停止拨弹
5. 保存系统上一次状态
6. 向消息中心发布：
   - `shoot_cmd`
   - `gimbal_cmd`

从调用关系上看：

- 上游输入先进入 `robot_cmd`
- `robot_cmd` 负责把输入变成统一控制命令
- 下游 `gimbal` 和 `shoot` 再根据这些命令执行自己的控制逻辑

因此，`robot_cmd` 是应用层消息流的中心节点。

## 如何被使用

本目录中的代码在当前工程中的使用方式如下：

### 1. 初始化阶段

在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中，`RobotInit()` 会调用：

- `RobotCMDInit()`

这样 `robot_cmd` 就会完成遥控器接口初始化和消息话题注册。

### 2. 运行阶段

在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中，`RobotTask()` 会周期性调用：

- `RobotCMDTask()`

因此 `robot_cmd` 的执行频率取决于应用层核心任务的调度节拍。

### 3. 下游消费方式

当前下游应用的使用方式为：

- [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 在初始化时订阅 `gimbal_cmd`，并在任务中读取该命令
- [shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c) 在初始化时订阅 `shoot_cmd`，并在任务中读取该命令

同时，它们也会将自身状态发布回 `robot_cmd`：

- `gimbal` 发布 `gimbal_feed`
- `shoot` 发布 `shoot_feed`

## 当前目录职责

`robot_cmd` 在当前工程中的定位是：

- 控制输入接收层
- 应用层状态决策层
- 云台与发射机构命令生成层
- 应用层消息流中心节点

它不是执行机构驱动层，也不直接保存电机控制闭环。

## 维护约束

维护本目录中的文件时，建议遵守以下约束：

- 输入设备解析逻辑统一放在 `robot_cmd.c` 中维护
- 发布和订阅的数据结构统一使用 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h) 中的定义
- 不在本目录重新引入底盘控制逻辑
- 不在本目录重新引入双板通信逻辑
- 新增控制模式时，优先扩展现有系统状态与输入分发流程，而不是绕开 `RobotCMDTask()`

## 备注

本说明文件只覆盖当前目录中的源码与头文件。  
文中提到其他目录中的文件，仅用于说明 `robot_cmd` 的调用关系、依赖来源和使用方式，不替代对应目录自己的说明文件。
