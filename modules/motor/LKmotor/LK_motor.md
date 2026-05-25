# LKmotor 模块说明

## 基本信息

- 士继 DREAMER 战队
- 作者：Xun Wei
- 参考文献：湖南大学跃鹿战队相关开源代码与文档
- 本工程为 RoboMaster 无人机云台部分代码

## 文件夹作用

`LKmotor` 文件夹用于封装 LK 电机在本工程中的控制逻辑。  
当前工程实际使用的是 `LK4005`，并且只用于云台俯仰轴控制。

该目录对上层应用隐藏了 LK 电机的 CAN 报文格式、反馈解析、启停控制和闭环计算方式。上层模块只需要完成初始化、切换反馈源并设置目标值即可。

## 当前目录文件说明

### `LK4005.h`

头文件，定义了 LK4005 电机的测量结构体、实例结构体和对外接口。

其中主要内容包括：

- `LKMotor_Measure_t`
  用于保存编码器、角度、转速、电流、温度和多圈角度等反馈信息。
- `LKMotorInstance`
  用于表示一个已注册的 LK 电机实例，内部包含：
  - 电机反馈数据
  - 电机控制配置
  - PID 控制器
  - 反馈与前馈指针
  - 重力补偿参数
  - CAN 实例
  - 守护实例
- `LKMotorInit()`
  用于初始化并注册一个 LK 电机实例。
- `LKMotorChangeFeed()`
  用于切换速度环或角度环的反馈来源。
- `LKMotorSetRef()`
  用于设置电机参考输入。
- `LKMotorControl()`
  由电机控制任务周期调用，统一完成 LK 电机控制计算和报文发送。
- `LKMotorStop()`、`LKMotorEnable()`
  用于控制 LK 电机启停。
- `LKMotorIsOnline()`
  用于查询电机是否在线。

### `LK4005.c`

源文件，完成 LK4005 电机的具体实现。

其核心逻辑包括：

- 在 `LKMotorInit()` 中完成电机实例分配、CAN 注册和守护注册
- 在 `LKMotorDecode()` 中解析 LK 电机反馈报文，得到角度、速度、电流和温度数据
- 在 `LKMotorControl()` 中遍历所有已注册 LK 电机，根据配置执行角度环和速度环计算，再将结果写入发送缓冲并发出控制报文
- 在 `LKMotorFillMailBox()` 中按照 LK 协议格式封装控制报文

## 在工程中的使用位置

当前工程中，`LKmotor` 模块只被云台应用直接使用。

### 在云台模块中的使用

在 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 中：

- 通过 `LKMotorInit()` 初始化俯仰轴 `LK4005`
- 通过 `LKMotorEnable()` 与 `LKMotorStop()` 控制俯仰轴启停
- 通过 `LKMotorChangeFeed()` 在不同模式下切换反馈来源
  - IMU 模式下使用 `IMU_FEED`
  - 电机反馈模式下使用 `MOTOR_FEED`
- 通过 `LKMotorSetRef()` 设置俯仰轴目标值
- 通过读取 `pitch_motor->measure.total_angle` 向上层回传俯仰轴电机角度

这说明 `LK4005` 在当前工程中承担的是云台俯仰执行器的角色。

### 在指令生成模块中的关联

在 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 中：

- 俯仰目标值 `pitch` 会根据当前控制模式被持续更新
- 电机反馈模式下会使用 `pitch_motor_angle` 作为控制参考
- IMU 模式下会使用姿态角 `Pitch` 作为控制参考

虽然 `robot_cmd` 不直接调用 `LKMotor` 接口，但它生成的 `gimbal_cmd.pitch` 最终会被 `gimbal` 模块传给 `LKMotorSetRef()`，因此该模块的控制语义与 `robot_cmd` 中的俯仰目标生成逻辑是直接关联的。

### 在电机任务中的使用

在 [motor_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/motor_task.c) 中：

- `LKMotorControl()` 被周期调用

这意味着 `gimbal` 模块不会自己发 LK 电机 CAN 报文，而是将目标值写入实例，再由统一电机任务按固定周期完成控制计算和报文发送。

## 与其他模块的关系

`LKmotor` 模块当前主要依赖以下模块：

- [motor_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/motor_def.h)
  提供通用电机控制配置和控制相关枚举
- [controller.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/algorithm/controller.h)
  提供 PID 初始化和 PID 计算接口
- [bsp_can.h](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/can/bsp_can.h)
  提供 CAN 注册、发送与接收回调能力
- [daemon.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/daemon/daemon.h)
  提供掉线检测能力
- [bsp_dwt.h](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/dwt/bsp_dwt.h)
  提供反馈周期测量能力

同时，该模块的反馈切换机制与 `imu` 模块和云台模式切换逻辑直接相关，因为俯仰轴既可能使用电机自身反馈，也可能使用 IMU 姿态反馈。

## 使用方式说明

在当前工程中，使用 LK4005 的一般流程如下：

1. 在应用初始化阶段构造 `Motor_Init_Config_s`
2. 调用 `LKMotorInit()` 获取电机实例指针
3. 在任务中根据模式调用：
   - `LKMotorChangeFeed()` 切换反馈来源
   - `LKMotorSetRef()` 设置目标值
   - `LKMotorEnable()` 或 `LKMotorStop()` 控制启停
4. 由 [motor_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/motor_task.c) 周期调用 `LKMotorControl()` 完成统一控制

## 设计说明

当前这版实现有两个和本工程使用方式直接相关的设计点。

### 当前工程按单俯仰电机场景使用

虽然代码内部保留了最多四个 LK 电机实例的管理能力，但当前工程实际只注册了一个 `LK4005`，即云台俯仰轴。因此这里的实现重点不是多电机协同，而是稳定完成单俯仰轴控制。

### 保留了反馈切换和重力补偿入口

俯仰轴和偏航轴不同，受到重力影响更明显，因此该模块在实例中保留了：

- 反馈来源切换能力
- `gravity` 重力补偿量

当前 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 中已经预留了俯仰轴重力补偿注释位置，后续若需要增加前馈补偿，可以在现有接口基础上继续扩展。

## 备注

- 当前工程实际使用的 LK 电机只有 `LK4005`
- 当前工程中该模块只服务云台俯仰轴
- 该目录的说明仅针对当前文件夹内的源码文件编写
