# algorithm 模块说明

## 基本信息

- 士继 DREAMER 战队
- 作者：Xun Wei
- 参考文献：湖南大学跃鹿战队相关开源代码与文档
- 本工程为 RoboMaster 无人机云台部分代码

## 文件夹作用

`algorithm` 文件夹用于提供本工程公共算法能力。  
它本身不直接连接硬件，也不直接承担应用层业务，而是为电机控制、姿态解算、通信校验和常用数学处理提供底层算法支持。

当前目录中的算法主要分为四类：

- PID 控制相关
- 卡尔曼滤波与四元数姿态解算相关
- CRC 校验相关
- 通用数学与工具函数相关

## 当前目录文件说明

### `controller.c` / `controller.h`

用于实现 PID 控制器。

主要提供：

- `PID_Init_Config_s`
  PID 初始化配置结构体
- `PIDInstance`
  PID 实例结构体
- `PIDInit()`
  初始化 PID 控制器
- `PIDCalculate()`
  根据测量值和参考值计算控制输出

当前实现中还包含若干可选改进环节，例如：

- 积分限幅
- 变速积分
- 梯形积分
- 微分先行
- 输出滤波
- 微分滤波
- 堵转错误检测

这是当前工程中最核心、使用最广的算法文件之一。

### `kalman_filter.c` / `kalman_filter.h`

用于实现通用卡尔曼滤波器框架。

该文件提供了通用状态空间滤波流程，包括：

- 状态预测
- 协方差预测
- 卡尔曼增益计算
- 状态更新
- 协方差更新

它本身不是直接服务某个具体硬件，而是作为更高层滤波算法的基础支撑。

### `QuaternionEKF.c` / `QuaternionEKF.h`

用于实现基于扩展卡尔曼滤波的四元数姿态解算。

它建立在 `kalman_filter` 之上，并面向 IMU 姿态估计场景做了专门封装。  
当前工程中的姿态解算主链就是基于这一部分实现的。

### `crc8.c` / `crc8.h`

用于实现 CRC8 校验。

### `crc16.c` / `crc16.h`

用于实现 CRC16 校验。

这两组文件主要服务于通信协议的数据校验场景。

### `user_lib.c` / `user_lib.h`

用于提供通用数学和工具函数。

这类文件通常用于放置多个模块都会用到、但又不适合放进具体驱动或应用层的辅助函数，例如：

- 限幅
- 单位转换
- 常见数学辅助函数
- 快速判断与通用处理逻辑

## 在工程中的使用位置

### PID 控制器的使用

`controller` 当前被多个电机驱动和 IMU 温控逻辑直接使用。

在以下文件中可以看到 `PIDInit()` 和 `PIDCalculate()` 的直接调用：

- [dji_motor.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DJImotor/dji_motor.c)
- [LK4005.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/LKmotor/LK4005.c)
- [dmmotor.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DMmotor/dmmotor.c)
- [ins_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/ins_task.c)

这说明当前工程中的：

- 云台偏航控制
- 云台俯仰控制
- 发射机构电机控制
- BMI088 温度控制

都直接建立在 `controller` 提供的 PID 算法之上。

### 姿态解算相关算法的使用

`QuaternionEKF` 当前主要被 IMU 模块使用。

在 [ins_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/ins_task.c) 中：

- `IMU_QuaternionEKF_Init()` 用于初始化姿态滤波器
- `IMU_QuaternionEKF_Update()` 用于周期更新姿态估计

同时，`QuaternionEKF` 的内部实现又依赖 `kalman_filter` 提供的通用滤波框架。

这说明在当前工程里：

- `kalman_filter` 是通用底层
- `QuaternionEKF` 是面向 IMU 的具体算法封装
- `imu` 模块是最终使用者

### CRC 的使用

从当前代码结构看，CRC 模块主要用于通信相关模块的数据校验。  
虽然当前云台主控制链不直接调用 CRC 接口，但通信协议类目录保留这些基础校验能力仍然是合理的。

### `user_lib` 的使用

在 [ins_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/ins_task.c) 和 [dmmotor.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DMmotor/dmmotor.c) 中都能看到对 `user_lib` 的引用。

这说明它承担的是公共辅助函数库的角色，而不是面向某一个模块单独存在。

## 与其他模块的关系

`algorithm` 模块当前与以下目录关系最紧密：

- [motor](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/motor.md)
  电机驱动层直接使用 PID 控制算法
- [imu](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/imu.md)
  姿态解算直接建立在四元数 EKF 和卡尔曼滤波之上
- [referee](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/referee)
  通信协议类代码通常需要 CRC 校验支持

它与应用层没有直接控制关系，而是通过 `motor`、`imu` 等模块间接支撑云台与发射控制链。

## 使用方式说明

当前工程对 `algorithm` 模块的使用方式可以概括为：

1. 驱动或功能模块引用对应算法头文件
2. 在初始化阶段完成算法实例配置
3. 在周期任务中调用对应算法更新输出

例如：

- 电机模块初始化阶段调用 `PIDInit()`
- 电机控制任务或 IMU 任务中调用 `PIDCalculate()`
- IMU 初始化时调用 `IMU_QuaternionEKF_Init()`
- IMU 周期任务中调用 `IMU_QuaternionEKF_Update()`

因此，`algorithm` 的典型使用者不是最终应用层，而是模块层。

## 设计说明

当前这版 `algorithm` 目录体现出两个明显特点。

### 算法与业务解耦

无论是 PID、EKF 还是 CRC，都没有直接写死到云台或发射业务中，而是先放在独立算法层，再由 `motor`、`imu` 等模块按需引用。

这样设计的好处是：

- 复用性更好
- 不同模块共享同一套基础能力
- 后续调整算法实现时不会直接打散业务结构

### 当前主要服务两条主链

虽然目录里包含多类算法文件，但对当前工程最重要的实际只有两条主链：

- 电机控制链：`controller`
- 姿态解算链：`kalman_filter + QuaternionEKF`

CRC 和 `user_lib` 更偏向基础支撑。

## 备注

- 当前目录中没有专门面向底盘的算法文件
- 原有文档中提到的部分历史内容已不再适用于当前工程
- 该目录的说明仅针对当前文件夹内的源码文件编写
