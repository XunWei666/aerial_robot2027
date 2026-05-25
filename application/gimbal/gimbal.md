# gimbal

<p align='right'>士继 DREAMER 战队</p>
<p align='right'>作者：Xun Wei</p>
<p align='right'>参考文献：湖南大学跃鹿战队相关开源代码与文档</p>

## 说明

本目录是 **RoboMaster 无人机云台部分代码** 中的云台应用目录。  
本说明文件只以当前目录中的 `.c` 与 `.h` 文件为核心进行说明，但会结合其上下游调用关系，说明这些文件在整套控制链中的作用和使用方式。

当前目录内需要说明的文件如下：

- `gimbal.c`
- `gimbal.h`

## 在工程中的位置

`gimbal` 是当前应用层中负责云台执行与状态反馈的应用模块。

它在工程中的上下游关系如下：

- 在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中由 `RobotInit()` 调用 `GimbalInit()` 完成初始化
- 在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中由 `RobotTask()` 周期性调用 `GimbalTask()`
- 控制命令由 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 生成并通过消息中心发布
- 使用的数据结构定义在 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h)
- 姿态数据来自 `INS_Init()` 返回的 IMU 解算结果
- 执行结果通过消息中心发布回 `robot_cmd`

因此，`gimbal` 的职责是消费上游控制命令，完成云台执行与反馈，而不是做控制输入决策。

## 文件关系

### `gimbal.h`

该文件对外声明 `gimbal` 应用的两个入口函数：

- `GimbalInit()`
- `GimbalTask()`

它的作用是向应用层总入口暴露云台应用的初始化接口与周期任务接口，使 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 可以统一调度云台应用。

### `gimbal.c`

该文件实现云台电机初始化、反馈源切换、模式执行和状态反馈发布逻辑。

从整体职责上看，它完成以下工作：

1. 初始化 yaw 与 pitch 执行机构
2. 关联 IMU 解算数据作为附加反馈源
3. 从消息中心读取 `gimbal_cmd`
4. 根据命令切换控制模式
5. 输出当前云台反馈到 `gimbal_feed`

它不负责生成目标角度，而是负责按照上游给出的目标执行控制。

## 初始化逻辑

`GimbalInit()` 主要完成三部分工作。

### 1. 获取姿态解算数据

当前通过：

- `INS_Init()`

获取云台姿态数据指针，并将其作为后续云台控制中 IMU 反馈的来源。

这意味着当前云台应用并不自己维护一套姿态解算，而是直接依赖 IMU 模块提供的统一姿态结果。

### 2. 初始化 yaw 与 pitch 电机

当前代码中：

- yaw 轴使用 `DJIMotorInit()`
- pitch 轴使用 `LKMotorInit()`

同时会为两个电机分别配置：

- CAN 参数
- 角度环参数
- 速度环参数
- 外环与内环类型
- 反馈来源

其中最关键的设置是：

- yaw 电机的 `other_angle_feedback_ptr` 指向 `YawTotalAngle`
- pitch 电机的 `other_angle_feedback_ptr` 指向 `Pitch`

这样在切换到 IMU 反馈模式时，电机控制器可以直接使用姿态解算数据作为反馈源。

### 3. 注册消息中心话题

当前 `gimbal` 在消息中心中承担“一收一发”的角色：

- 订阅 `gimbal_cmd`
- 发布 `gimbal_feed`

对应的数据类型都来自 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h)：

- `Gimbal_Ctrl_Cmd_s`
- `Gimbal_Upload_Data_s`

而这些命令与反馈的另一端在 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 中完成注册和使用。

## 周期任务逻辑

`GimbalTask()` 是云台应用的核心周期任务。

当前执行流程如下：

1. 从消息中心读取 `gimbal_cmd`
2. 根据 `gimbal_mode` 判断当前控制模式
3. 按照模式切换电机反馈源与启停状态
4. 将当前姿态与电机状态整理为反馈结构
5. 发布 `gimbal_feed`

## 控制模式逻辑

当前云台应用支持以下三种模式。

### `GIMBAL_ZERO_FORCE`

该模式下：

- 停止 yaw 电机
- 停止 pitch 电机

通常由上游在急停或安全待机时下发。

### `GIMBAL_GYRO_MODE`

该模式下：

- 使能 yaw 与 pitch 电机
- 将 yaw 与 pitch 的角度环、速度环反馈源切换为 IMU 反馈
- 使用上游下发的角度目标作为控制参考值

这里的命令来源于 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c)。  
在那里，上游会先根据当前反馈把目标值转换到合适的角度参考系后，再发给 `gimbal`。

### `GIMBAL_SELF_MODE`

该模式下：

- 使能 yaw 与 pitch 电机
- 将反馈源切换回电机自身编码器反馈
- 使用上游下发的编码器参考值执行控制

也就是说，`gimbal` 本身并不决定目标值属于 IMU 坐标还是编码器坐标，而是根据上游命令中的模式切换执行方式。

## 反馈数据

当前 `gimbal` 发布的反馈数据包括：

- `gimbal_imu_data`
- `yaw_motor_angle`
- `pitch_motor_angle`
- `gimbal_mode`

这些数据会被 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 读取，用于：

- 模式切换时对齐参考值
- 软限位处理
- 控制目标连续性维护

因此，`gimbal_feed` 不只是状态上报，它实际上也是上游生成下一拍控制量的重要依据。

## 如何被使用

本目录中的代码在当前工程中的使用方式如下：

### 1. 初始化阶段

在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中，`RobotInit()` 会调用：

- `GimbalInit()`

这样云台应用会完成电机初始化、IMU 数据接入和消息注册。

### 2. 运行阶段

在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中，`RobotTask()` 会周期性调用：

- `GimbalTask()`

这样云台应用会持续读取上游命令并发布反馈。

### 3. 上游使用方式

在 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 中：

- `RobotCMDInit()` 会注册 `gimbal_cmd` 和 `gimbal_feed`
- `RobotCMDTask()` 会发布 `gimbal_cmd`
- `RobotCMDTask()` 也会读取 `gimbal_feed`

因此，`gimbal` 与 `robot_cmd` 共同构成当前工程中的云台控制闭环。

## 当前目录职责

`gimbal` 在当前工程中的定位是：

- 云台执行机构初始化层
- 云台控制模式执行层
- 云台反馈整理与发布层

它不是输入解析层，也不是整车状态决策层。

## 维护约束

维护本目录中的文件时，建议遵守以下约束：

- 云台命令和反馈结构统一使用 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h) 中的定义
- 上游命令来源统一视为消息中心，不在本目录直接解析遥控器或键鼠输入
- IMU 与电机反馈源的切换逻辑统一维护在 `gimbal.c` 中
- 新增云台模式时，应同时明确上游命令语义和当前目录下的执行方式
- 不在本目录重新引入底盘控制或双板通信逻辑

## 备注

本说明文件只覆盖当前目录中的源码与头文件。  
文中提到其他目录中的文件，仅用于说明 `gimbal` 的调用关系、依赖来源和使用方式，不替代对应目录自己的说明文件。
