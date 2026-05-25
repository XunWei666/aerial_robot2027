# shoot

<p align='right'>士继 DREAMER 战队</p>
<p align='right'>作者：Xun Wei</p>
<p align='right'>参考文献：湖南大学跃鹿战队相关开源代码与文档</p>

## 说明

本目录是 **RoboMaster 无人机云台部分代码** 中的发射机构应用目录。  
本说明文件只以当前目录中的 `.c` 与 `.h` 文件为核心进行说明，但会结合其上下游调用关系，说明这些文件在整套控制链中的作用和使用方式。

当前目录内需要说明的文件如下：

- `shoot.c`
- `shoot.h`

## 在工程中的位置

`shoot` 是当前应用层中负责发射机构执行与状态反馈的应用模块。

它在工程中的上下游关系如下：

- 在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中由 `RobotInit()` 调用 `ShootInit()` 完成初始化
- 在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中由 `RobotTask()` 周期性调用 `ShootTask()`
- 控制命令由 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 生成并通过消息中心发布
- 使用的数据结构定义在 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h)
- 执行结果通过消息中心发布回 `robot_cmd`

因此，`shoot` 的职责是消费上游控制命令，完成摩擦轮与拨弹机构的执行，并把执行反馈回传给上游。

## 文件关系

### `shoot.h`

该文件对外声明 `shoot` 应用的两个入口函数以及本目录内部使用的关键宏参数。

对外入口包括：

- `ShootInit()`
- `ShootTask()`

同时，头文件中还定义了当前发射机构控制中会使用到的关键参数，例如：

- `MAX_FRIC_SPEED_RPM`
- `LOADER_MAX_CURRENT`
- `LOAD_REVERSR_SPEED`
- `JAM_DETECT_TIME`
- `JAM_REVERSE_TIME`

这些宏用于约束摩擦轮目标转速、卡弹检测阈值和反转持续时间，是当前发射机构控制逻辑的重要组成部分。

### `shoot.c`

该文件实现发射机构电机初始化、摩擦轮控制、拨弹逻辑和反馈发布逻辑。

从整体职责上看，它完成以下工作：

1. 初始化左右摩擦轮电机与拨弹电机
2. 从消息中心读取 `shoot_cmd`
3. 根据控制命令执行摩擦轮控制
4. 根据控制命令执行拨弹控制
5. 采集拨弹电机反馈并发布 `shoot_feed`

它不负责判断何时开火，也不负责卡弹决策来源，这些策略由上游 `robot_cmd` 生成命令后再交给 `shoot` 执行。

## 初始化逻辑

`ShootInit()` 主要完成两部分工作。

### 1. 初始化执行机构

当前发射机构包含三个电机实例：

- 左摩擦轮电机
- 右摩擦轮电机
- 拨弹电机

其中：

- 摩擦轮使用 `M3508`
- 拨弹机构使用 `M2006`

初始化时会为它们分别配置：

- CAN 句柄与发送 ID
- 速度环参数
- 角度环参数
- 闭环类型
- 电机方向

其中拨弹电机初始化为 `SPEED_LOOP`，目的是避免上电瞬间因角度环参考值未准备好而出现异常转动。

### 2. 注册消息中心话题

当前 `shoot` 在消息中心中承担“一收一发”的角色：

- 订阅 `shoot_cmd`
- 发布 `shoot_feed`

对应的数据类型都来自 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h)：

- `Shoot_Ctrl_Cmd_s`
- `Shoot_Upload_Data_s`

这些话题的另一端在 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 中完成注册和使用。

## 周期任务逻辑

`ShootTask()` 是发射机构应用的核心周期任务。

当前执行流程如下：

1. 从消息中心读取 `shoot_cmd`
2. 根据命令控制摩擦轮
3. 根据命令控制拨弹电机
4. 整理拨弹电机反馈
5. 发布 `shoot_feed`

当前 `shoot` 并不直接处理复杂状态机，而是把上游给出的模式命令具体落实到执行机构上。

## 摩擦轮控制逻辑

当前摩擦轮控制的核心输入为：

- `friction_mode`
- `bullet_speed`

当 `friction_mode == FRICTION_OFF` 时：

- 若软启动速度仍大于零，则逐步降速
- 当软启动速度降为零后，停止摩擦轮电机

当 `friction_mode == FRICTION_ON` 时：

- 使能左右摩擦轮电机
- 根据 `bullet_speed` 选择目标转速
- 通过软启动方式逐步提升到目标速度

这里保留软启动的目的，是避免摩擦轮瞬时大电流冲击。

## 拨弹控制逻辑

当前拨弹控制的核心输入为：

- `load_mode`
- `shoot_freq`

当前支持的拨弹模式包括：

- `LOAD_STOP`
- `LOAD_1_BULLET`
- `LOAD_BURSTFIRE`
- `LOAD_REVERSE`

对应逻辑如下：

### `LOAD_STOP`

- 清除单发状态
- 维持拨弹电机在速度环
- 参考值置零

### `LOAD_1_BULLET`

- 若尚未进入单发状态，则记录一次新的单发目标角度
- 将拨弹电机切换到角度环
- 参考值设为目标角度

这里使用：

- `ONE_BULLET_DELTA_ANGLE`
- `REDUCTION_RATIO_LOADER`

共同计算单发对应的拨弹盘目标角度。

### `LOAD_BURSTFIRE`

- 清除单发状态
- 切换到速度环
- 参考值根据 `shoot_freq` 换算为拨弹速度

### `LOAD_REVERSE`

- 清除单发状态
- 切换到速度环
- 以固定反转速度退弹

## 反馈数据

当前 `shoot` 发布的反馈数据内容较精简，主要是：

- `loader_data`

也就是拨弹电机当前测量值。

这些反馈会被 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 读取，用于：

- 卡弹检测
- 反转退弹逻辑判断

因此，虽然当前 `shoot_feed` 只有一项核心反馈，但它对上游发射决策是必需的。

## 如何被使用

本目录中的代码在当前工程中的使用方式如下：

### 1. 初始化阶段

在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中，`RobotInit()` 会调用：

- `ShootInit()`

这样发射机构应用会完成执行机构初始化和消息注册。

### 2. 运行阶段

在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中，`RobotTask()` 会周期性调用：

- `ShootTask()`

这样发射机构应用会持续读取上游命令并输出反馈。

### 3. 上游使用方式

在 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 中：

- `RobotCMDInit()` 会注册 `shoot_cmd` 和 `shoot_feed`
- `RobotCMDTask()` 会发布 `shoot_cmd`
- `RobotCMDTask()` 也会读取 `shoot_feed`

其中，`robot_cmd` 会利用 `shoot_feed.loader_data.real_current` 执行卡弹检测与反转决策，再通过 `shoot_cmd` 回写给 `shoot` 执行。

因此，`shoot` 与 `robot_cmd` 共同构成当前工程中的发射机构控制闭环。

## 当前目录职责

`shoot` 在当前工程中的定位是：

- 发射机构执行机构初始化层
- 摩擦轮与拨弹控制执行层
- 发射机构反馈整理与发布层

它不是输入解析层，也不是上游发射策略决策层。

## 维护约束

维护本目录中的文件时，建议遵守以下约束：

- 发射机构命令和反馈结构统一使用 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h) 中的定义
- 上游命令来源统一视为消息中心，不在本目录直接解析遥控器或键鼠输入
- 卡弹判定阈值和反转时间统一维护在 `shoot.h` 中
- 新增发射模式时，应同时明确上游命令语义和当前目录下的执行方式
- 不在本目录重新引入底盘控制或双板通信逻辑

## 备注

本说明文件只覆盖当前目录中的源码与头文件。  
文中提到其他目录中的文件，仅用于说明 `shoot` 的调用关系、依赖来源和使用方式，不替代对应目录自己的说明文件。
