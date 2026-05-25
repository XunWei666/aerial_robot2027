# DJImotor 模块说明

## 基本信息

- 士继 DREAMER 战队
- 作者：Xun Wei
- 参考文献：湖南大学跃鹿战队相关开源代码与文档
- 本工程为 RoboMaster 无人机云台部分代码

## 文件夹作用

`DJImotor` 文件夹用于封装 DJI 系列电机在本工程中的控制逻辑，当前实际覆盖三类电机：

- `GM6020`：用于云台偏航轴
- `M3508`：用于发射机构摩擦轮
- `M2006`：用于发射机构拨弹盘

该目录对上层应用隐藏了 CAN 报文收发、反馈解析、串级 PID 计算和多电机分组发送等底层细节。上层模块只需要完成电机初始化，并在任务中设置目标值即可。

## 当前目录文件说明

### `dji_motor.h`

头文件，定义了 DJI 电机模块的对外接口、测量结构体和电机实例结构体。

其中主要内容包括：

- `DJI_Motor_Measure_s`
  用于保存编码器、转速、电流、温度、多圈角度等反馈数据。
- `DJIMotorInstance`
  用于表示一个已注册的 DJI 电机实例，内部包含：
  - 电机反馈数据
  - 电机控制配置
  - PID 控制器
  - 绑定的 CAN 实例
  - 分组发送信息
  - 守护实例
- `DJIMotorInit()`
  用于注册并初始化一个 DJI 电机实例。
- `DJIMotorSetRef()`
  用于设置电机参考输入。
- `DJIMotorChangeFeed()`
  用于切换速度环或角度环的反馈来源。
- `DJIMotorOuterLoop()`
  用于切换当前最外层闭环目标。
- `DJIMotorEnable()` 与 `DJIMotorStop()`
  用于控制电机是否向总线输出控制量。
- `DJIMotorControl()`
  由电机控制任务周期调用，统一完成所有已注册 DJI 电机的控制计算和 CAN 发送。

### `dji_motor.c`

源文件，完成 DJI 电机模块的具体实现。

其核心逻辑包括：

- 在 `DJIMotorInit()` 中为每个电机分配实例并完成注册
- 在 `MotorSenderGrouping()` 中根据电机类型、CAN 总线和电机 ID 自动计算反馈 ID，并将电机分配到对应发送组
- 在 `DecodeDJIMotor()` 中解析电机反馈报文，得到角度、转速、电流和温度信息
- 在 `DJIMotorControl()` 中遍历所有已注册电机，按配置执行角度环、速度环、电流环计算，再按发送组统一发出控制报文

## 在工程中的使用位置

当前工程中，`DJImotor` 模块被两个应用模块直接使用。

### 在云台模块中的使用

在 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 中：

- 通过 `DJIMotorInit()` 初始化偏航轴 `GM6020`
- 通过 `DJIMotorChangeFeed()` 在不同模式下切换反馈来源
  - IMU 模式下使用 `IMU_FEED`
  - 电机编码器模式下使用 `MOTOR_FEED`
- 通过 `DJIMotorSetRef()` 设置偏航轴目标
- 通过 `DJIMotorEnable()` 与 `DJIMotorStop()` 控制偏航轴启停

这说明 `GM6020` 在当前工程中承担的是云台偏航执行器的角色。

### 在发射模块中的使用

在 [shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c) 中：

- 通过 `DJIMotorInit()` 初始化左右摩擦轮 `M3508`
- 通过 `DJIMotorInit()` 初始化拨弹盘 `M2006`
- 通过 `DJIMotorSetRef()` 设置摩擦轮转速和拨弹盘目标
- 通过 `DJIMotorOuterLoop()` 切换拨弹盘在单发时的位置环模式和连发时的速度环模式
- 通过 `DJIMotorEnable()` 与 `DJIMotorStop()` 控制摩擦轮与拨弹盘启停

这说明 `DJImotor` 模块也承担了发射机构执行器的底层控制。

### 在电机任务中的使用

在 [motor_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/motor_task.c) 中：

- `DJIMotorControl()` 被周期调用

这意味着应用层不会直接执行 PID 计算和 CAN 发送，而是把目标值写入电机实例，由统一的电机控制任务按固定周期完成闭环控制。

## 与其他模块的关系

`DJImotor` 模块当前主要依赖以下模块：

- [motor_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/motor_def.h)
  提供通用电机控制配置、控制器结构体和枚举类型
- [controller.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/algorithm/controller.h)
  提供 PID 初始化和 PID 计算接口
- [bsp_can.h](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/can/bsp_can.h)
  提供 CAN 注册、发送和回调机制
- [daemon.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/daemon/daemon.h)
  提供电机掉线检测能力
- [bsp_dwt.h](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/dwt/bsp_dwt.h)
  提供时间间隔计算，用于速度更新和控制周期统计

同时，该模块向上层应用提供统一的“电机对象 + 目标值接口”控制方式，使 `gimbal` 和 `shoot` 不需要关心底层 CAN 报文细节。

## 使用方式说明

在当前工程中，使用 DJI 电机的一般流程如下：

1. 在应用初始化阶段构造 `Motor_Init_Config_s`
2. 调用 `DJIMotorInit()` 获取电机实例指针
3. 在任务中根据模式调用：
   - `DJIMotorSetRef()` 设置目标值
   - `DJIMotorOuterLoop()` 切换最外层闭环
   - `DJIMotorChangeFeed()` 切换反馈来源
   - `DJIMotorEnable()` 或 `DJIMotorStop()` 控制启停
4. 由 [motor_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/motor_task.c) 周期调用 `DJIMotorControl()` 完成统一控制

## 设计说明

当前这版实现有两个和使用者强相关的设计点。

### 多电机分组发送

DJI 电机控制报文不是“一台电机一帧”，而是同一组内最多四台电机共用一帧控制报文。因此模块内部专门维护了发送分组，并在初始化时自动完成分组映射。这样应用层初始化电机时只需要填写电机 ID 和 CAN 总线，不需要手工计算发送 ID 和接收 ID。

### 反馈来源可切换

角度环和速度环的反馈既可以来自电机自身，也可以来自外部传感器。在当前工程中，这一机制主要服务于云台模式切换：

- 使用电机反馈时，适合基于编码器的控制
- 使用 IMU 反馈时，适合基于姿态角的控制

因此 `DJIMotorChangeFeed()` 是当前云台模块的重要接口之一。

## 备注

- 当前工程真正使用到的 DJI 电机类型只有 `GM6020`、`M3508` 和 `M2006`
- 该目录的说明仅针对当前文件夹内的源码文件编写
- 具体电机参数配置应结合 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 与 [shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c) 中的初始化代码阅读
