# imu

<p align='right'>士继 DREAMER 战队</p>
<p align='right'>作者：Xun Wei</p>
<p align='right'>参考文献：湖南大学跃鹿战队相关开源代码与文档</p>

## 说明

本目录是 **RoboMaster 无人机云台部分代码** 中的 IMU 模块目录。

当前目录负责完成从 BMI088 原始传感器数据采集，到姿态解算结果输出的整条链路。它既包含底层传感器驱动，也包含硬件适配层和上层姿态任务逻辑，是当前云台控制链中最核心的感知模块之一。

当前目录中需要说明的文件包括：

- `ins_task.h`
- `ins_task.c`
- `BMI088driver.h`
- `BMI088driver.c`
- `BMI088Middleware.h`
- `BMI088Middleware.c`
- `BMI088reg.h`

## 在工程中的位置

本目录在当前工程中的主要调用关系如下：

- 在 [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h) 中，由 `StartINSTASK()` 调用 `INS_Init()` 和 `INS_Task()`
- 在 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 中，由 `GimbalInit()` 调用 `INS_Init()` 获取姿态数据指针
- 在 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 中，yaw 与 pitch 电机都将 IMU 数据作为闭环反馈来源之一
- 在 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h) 中，`Gimbal_Upload_Data_s` 会携带 `attitude_t`
- 在算法层依赖 [QuaternionEKF.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/algorithm/QuaternionEKF.c)、[controller.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/algorithm/controller.c)、[user_lib.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/algorithm/user_lib.c)
- 在底层依赖 `SPI`、`TIM`、`DWT` 和 `GPIO`

因此，本目录负责把底层 BMI088 数据变成应用层和控制层可以直接使用的姿态反馈。

## 当前目录文件关系

本目录内部可以分成三层。

### 1. 姿态任务层

对应文件：

- `ins_task.h`
- `ins_task.c`

这一层负责：

- IMU 总初始化
- 姿态解算主流程
- IMU 温度控制
- 对上层输出姿态结果

它是本目录对应用层最直接的接口层。

### 2. BMI088 驱动层

对应文件：

- `BMI088driver.h`
- `BMI088driver.c`
- `BMI088reg.h`

这一层负责：

- BMI088 初始化
- BMI088 原始加速度、角速度和温度读取
- 在线标定或离线参数装载
- 传感器寄存器配置

这一层把具体传感器协议细节封装起来，供 `ins_task` 使用。

### 3. 硬件适配层

对应文件：

- `BMI088Middleware.h`
- `BMI088Middleware.c`

这一层负责：

- 加速度计片选控制
- 陀螺仪片选控制
- 单字节 SPI 读写适配
- 保存当前使用的 SPI 句柄

它的作用是把驱动层和当前板级硬件连接起来。

## 当前目录文件说明

### `ins_task.h`

该文件定义 IMU 模块对外暴露的姿态结构体和接口。

当前最重要的结构体是：

- `attitude_t`

其中包含：

- `Gyro[3]`
- `Accel[3]`
- `Roll`
- `Pitch`
- `Yaw`
- `YawTotalAngle`

其中 `YawTotalAngle` 对云台多圈控制尤其重要，因此在 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 中被直接作为 yaw 轴的额外角度反馈来源。

该文件同时声明：

- `INS_Init()`
- `INS_Task()`
- 若干姿态与坐标变换辅助函数

### `ins_task.c`

该文件实现姿态任务逻辑。

当前主要负责：

1. 初始化 BMI088 与温控 PWM
2. 根据初始加速度估计初始四元数
3. 初始化四元数 EKF
4. 周期读取 BMI088 数据
5. 执行安装误差修正
6. 调用 EKF 更新姿态
7. 输出 `Yaw`、`Pitch`、`Roll` 和 `YawTotalAngle`
8. 周期控制 IMU 温度

从 [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h) 可见，`INS_Task()` 当前被设计为约 `1 kHz` 运行。

### `BMI088driver.h`

该文件定义 BMI088 驱动层的数据结构、灵敏度常量、错误码和对外接口。

当前最重要的内容包括：

- `IMU_Data_t`
- `BMI088` 全局数据对象声明
- `BMI088Init()`
- `bmi088_accel_init()`
- `bmi088_gyro_init()`
- `BMI088_Read()`

这一层直接面向 BMI088 设备本身，而不是面向上层应用逻辑。

### `BMI088driver.c`

该文件实现 BMI088 初始化、标定和周期数据读取逻辑。

当前主要完成：

- 保存 SPI 句柄
- 初始化加速度计和陀螺仪
- 执行在线标定或装载离线标定参数
- 读取加速度、角速度和温度
- 完成灵敏度换算和零偏补偿

当前 [ins_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/ins_task.c) 中读取原始传感器数据时，直接通过 `BMI088_Read()` 获取结果。

### `BMI088Middleware.h`

该文件定义 BMI088 驱动层所需的板级适配接口。

它对驱动层暴露：

- 片选拉低与拉高函数
- 单字节读写函数
- 当前 SPI 句柄指针

### `BMI088Middleware.c`

该文件实现当前板级的 BMI088 硬件访问方式。

它通过 HAL 层完成：

- 加速度计片选控制
- 陀螺仪片选控制
- 单字节 SPI 收发

驱动层并不直接关心具体 GPIO 和 HAL 细节，而是通过这一层完成硬件访问。

### `BMI088reg.h`

该文件仅负责 BMI088 寄存器地址、配置位和灵敏度相关常量定义。

它不提供业务逻辑，但它决定了 BMI088 驱动层的配置基础。

## 主要接口说明

### `INS_Init()`

该接口定义在 [ins_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/ins_task.h)，实现于 [ins_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/ins_task.c)。

当前主要职责包括：

- 初始化 BMI088
- 初始化姿态解算状态
- 初始化四元数 EKF
- 初始化 IMU 温控 PID
- 返回姿态数据指针

当前返回值类型为：

- `attitude_t *`

该返回值会被 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 保存下来，用作云台闭环控制反馈来源。

### `INS_Task()`

该接口同样定义在 [ins_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/ins_task.h)，实现于 [ins_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/ins_task.c)。

当前主要完成：

1. 周期读取 BMI088 数据
2. 更新姿态 EKF
3. 计算坐标变换结果
4. 更新运动加速度
5. 输出最新姿态角
6. 执行温控

它是当前工程姿态感知链路中的主循环。

### `BMI088Init()`

该接口定义于 [BMI088driver.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/BMI088driver.h)，实现于 [BMI088driver.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/BMI088driver.c)。

当前主要完成：

- 绑定 SPI 句柄
- 初始化加速度计和陀螺仪
- 根据参数决定是否在线标定

### `BMI088_Read()`

该接口负责：

- 读取加速度
- 读取角速度
- 读取温度
- 执行换算和补偿

它是 `INS_Task()` 的直接底层数据入口。

## 当前工程中的使用方式

### 在任务层中的使用

在 [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h) 中：

- `StartINSTASK()` 会调用 `INS_Init()`
- 随后在循环中周期执行 `INS_Task()`

因此本目录承担的是实时姿态任务，而不是一次性工具函数角色。

### 在云台应用中的使用

在 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 中：

- `GimbalInit()` 会调用 `INS_Init()` 获取姿态数据指针
- yaw 电机会把 `YawTotalAngle` 作为外部角度反馈来源
- pitch 电机会把 `Pitch` 作为外部角度反馈来源
- 两个电机还会使用 `Gyro[]` 作为速度反馈来源之一

这说明当前 `imu` 模块的主要服务对象就是云台控制链。

### 在应用消息中的使用

在 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h) 中：

- `Gimbal_Upload_Data_s` 包含 `attitude_t gimbal_imu_data`

也就是说，IMU 解算结果最终还会通过应用层消息反馈给 `robot_cmd`，用于模式切换和目标对齐。

## 当前目录职责

`imu` 在当前工程中的职责是：

- 获取 BMI088 原始数据
- 完成姿态解算
- 提供云台控制所需的姿态与角速度反馈
- 承担独立 IMU 实时任务

它不是业务决策模块，也不是应用层消息模块。

## 维护约束

维护本目录时，建议遵守以下约束：

- 姿态输出统一使用 `attitude_t`
- 上层应用只通过 `INS_Init()` 和 `INS_Task()` 使用本模块
- 驱动层与硬件适配层保持职责分离
- 修改姿态输出字段时，应同步检查 [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 和 [robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h) 的使用逻辑
- 若后续替换 BMI088 驱动实现，应保证上层 `attitude_t` 接口尽量保持稳定

## 备注

本说明文件只覆盖当前目录中的 `.c/.h` 文件。

文中提到 [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h)、[gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c)、[robot_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_def.h) 和 `algorithm` 目录，仅用于说明本目录的调用位置、依赖来源和使用方式，不替代对应目录自己的说明文件。
