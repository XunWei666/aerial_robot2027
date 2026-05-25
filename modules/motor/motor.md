# motor

<p align='right'>士继 DREAMER 战队</p>
<p align='right'>作者：Xun Wei</p>
<p align='right'>参考文献：湖南大学跃鹿战队相关开源代码与文档</p>

## 说明

本目录是 **RoboMaster 无人机云台部分代码** 中的电机模块根目录。

本目录本身不对应某一种具体电机，而是负责定义多类电机共用的控制结构，并将各类电机驱动的控制入口统一收束到一个周期任务中。当前工程中的云台偏航电机、云台俯仰电机、摩擦轮电机和拨弹电机，虽然分别由不同子目录下的驱动实现，但它们在初始化时都依赖本目录提供的通用配置结构，运行时也通过本目录的统一控制任务参与周期闭环。

当前目录中需要说明的文件包括：

- `motor_def.h`
- `motor_task.h`
- `motor_task.c`

## 在工程中的位置

本目录在当前工程中的主要调用关系如下：

- 在 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 中，先构造 `Motor_Init_Config_s`，再调用 `DJIMotorInit()` 和 `LKMotorInit()`
- 在 [shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c) 中，先构造 `Motor_Init_Config_s`，再调用 `DJIMotorInit()`
- 在 [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h) 中，由 `StartMOTORTASK()` 周期调用 `MotorControlTask()`

因此，本目录既是电机初始化参数的统一定义位置，也是不同电机驱动周期控制入口的统一调度层。

## 当前目录文件

### `motor_def.h`

该文件定义电机控制模块的通用枚举和通用数据结构，是各类电机驱动共同依赖的基础头文件。

主要内容包括：

- 闭环类型定义，例如开环、电流环、速度环、角度环
- 前馈类型定义
- 反馈来源定义，例如电机自身反馈和 IMU 反馈
- 电机转向与反馈方向设置
- 电机控制设置结构体
- 电机控制器初始化结构体
- 通用电机初始化结构体 `Motor_Init_Config_s`
- 电机类型枚举

在当前工程中，[gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 和 [shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c) 都会先构造 `Motor_Init_Config_s`，再调用具体电机驱动的初始化接口创建实例。因此该头文件的作用不是直接控制某个电机，而是统一描述“如何初始化一个电机控制实例”。

### `motor_task.h`

该文件声明统一电机控制任务：

- `MotorControlTask()`

这个任务的意义在于把不同类型电机的周期控制入口统一放到一处，避免应用层分别调度每一类电机驱动。

### `motor_task.c`

该文件实现统一电机控制任务。

当前实现中，`MotorControlTask()` 只顺序调用：

- `DJIMotorControl()`
- `LKMotorControl()`

这说明当前工程真正参与电机周期控制主链的主要只有 DJI 电机和 LK 电机两类。

## 当前工程中的使用方式

### 在云台模块中的使用

在 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 中：

- yaw 轴先构造 `Motor_Init_Config_s`，再通过 `DJIMotorInit()` 创建 GM6020 实例
- pitch 轴先构造 `Motor_Init_Config_s`，再通过 `LKMotorInit()` 创建 LK4005 实例

其中，yaw 和 pitch 都会把 IMU 姿态或角速度指针填入配置结构，作为闭环反馈来源的一部分。

### 在发射模块中的使用

在 [shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c) 中：

- 左右摩擦轮和拨弹电机都先构造 `Motor_Init_Config_s`
- 然后都通过 `DJIMotorInit()` 创建实例

因此，虽然发射机构当前只使用 DJI 电机驱动，但仍然依赖本目录定义的通用控制结构。

### 在任务调度中的使用

在 [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h) 中：

- `StartMOTORTASK()` 会周期执行 `MotorControlTask()`

因此本目录同时承担：

- 电机初始化参数公共定义
- 电机驱动统一调度入口

## 与子目录的关系

本目录只负责通用层，不负责某一类具体电机协议的细节实现。

当前保留的具体电机驱动子目录只有：

- [DJImotor](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DJImotor)
- [DMmotor](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DMmotor)
- [LKmotor](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/LKmotor)

这些子目录分别负责：

- 具体电机实例管理
- 反馈解析
- 闭环计算
- 报文发送

本目录的职责是用统一的数据结构和统一任务入口把这些驱动组织起来。

## 当前目录职责

`motor` 根目录在当前工程中的职责是：

- 定义通用电机控制结构
- 统一不同电机驱动的初始化参数格式
- 提供统一电机周期控制入口
- 作为云台与发射执行机构控制链的中间层

它不是具体电机协议驱动目录，而是电机系统的通用抽象层。

## 维护约束

维护本目录时，建议遵守以下约束：

- 通用闭环结构和枚举统一维护在 `motor_def.h`
- 应用层创建电机实例时统一通过 `Motor_Init_Config_s` 传参
- `motor_task.c` 只保留当前工程确实参与周期控制的驱动入口
- 新增或裁剪具体电机类型时，应同步检查本目录和对应子目录的关系说明

## 备注

本说明文件只覆盖当前目录中的 `.c/.h` 文件。

文中提到 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c)、[shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c)、[robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h) 以及三个电机子目录，仅用于说明本目录的调用位置和使用方式，不替代对应目录自己的说明文件。
