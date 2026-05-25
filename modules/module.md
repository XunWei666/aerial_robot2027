# modules

<p align='right'>士继 DREAMER 战队</p>
<p align='right'>作者：Xun Wei</p>
<p align='right'>参考文献：湖南大学跃鹿战队相关开源代码与文档</p>

## 说明

本目录是 **RoboMaster 无人机云台部分代码** 的模块层根目录。

`modules` 层位于 `application` 与 `bsp` 之间，负责把算法、传感器、执行机构、通信和系统辅助功能封装成可复用模块，供应用层组合使用。当前工程已经收敛为云台与发射机构单板控制，因此本目录中的模块也以服务云台姿态控制、发射机构控制和输入解析为主。

本说明文件只对 `modules` 根目录这一层做总览说明，不替代各子目录自己的说明文件。

## 当前目录文件

### `general_def.h`

该文件用于放置模块层共享的通用定义。

它的作用主要是：

- 为多个模块提供公共宏或基础类型定义
- 避免相同的通用定义在不同模块中重复出现
- 作为模块层的公共头文件之一，被上层应用和下层模块共同引用

### `module.md`

该文件是 `modules` 层总览说明文件，用于描述当前保留模块的组织方式、在工程中的位置以及它们的大致职责。

## 在工程中的位置

在当前工程中，`modules` 层主要被以下应用层文件直接使用：

- [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c)
- [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h)
- [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c)
- [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c)
- [shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c)

其中：

- `robot.c` 负责在初始化阶段拉起应用层，同时先初始化消息中心
- `robot_task.h` 负责创建姿态解算、电机控制、守护任务和机器人核心任务
- `robot_cmd.c` 依赖遥控器、消息中心和部分电机控制接口
- `gimbal.c` 依赖 IMU、消息中心和电机模块
- `shoot.c` 依赖消息中心和电机模块

因此，`modules` 层是当前工程控制链路中的核心支撑层。

## 当前保留的子目录

当前 `modules` 根目录下保留并有效参与工程组织的子目录如下：

- `alarm`
- `algorithm`
- `daemon`
- `imu`
- `ist8310`
- `message_center`
- `motor`
- `referee`
- `remote`

下面只对这些子目录在模块层中的定位做总览说明。

### `alarm`

用于封装蜂鸣器等声响提示功能。

当前工程中该目录仍被保留，但实际调用已经比较少，主要作为系统提示能力的预留模块存在。

### `algorithm`

用于封装控制和数学相关算法。

当前目录中包含 PID 控制器、四元数 EKF、滤波器、CRC 以及常用数学工具函数，是 IMU 解算和电机控制的基础支撑模块。

### `daemon`

用于封装守护与离线检测逻辑。

在当前工程中，守护任务会周期执行该模块提供的接口，用于监控外设或模块的在线状态。

### `imu`

用于封装当前工程实际使用的 IMU 链路。

该目录中包含：

- BMI088 驱动
- BMI088 硬件适配层
- 姿态解算任务

它直接服务于云台控制，是当前工程最核心的模块之一。

### `ist8310`

用于封装 IST8310 磁力计驱动。

当前工程主控制链路中并未直接使用该模块，但目录被保留，供后续需要接入磁力计时继续扩展。

### `message_center`

用于封装应用层之间的消息通信机制。

当前工程中的 `robot_cmd`、`gimbal` 和 `shoot` 之间不再直接相互调用，而是通过该模块发布和订阅消息。现在的消息中心采用“只保留最新值”的语义，适合当前控制任务只关心最新控制量和最新状态量的使用场景。

### `motor`

用于封装电机控制相关能力。

该目录既包含电机通用配置结构和统一电机控制任务，也包含具体电机驱动子目录。当前工程中真正参与控制链路的主要是 DJI 电机和 LK 电机。

### `referee`

用于封装裁判系统通信与 UI 相关能力。

当前工程中该模块仍被保留，但不属于云台与发射机构控制主链的核心部分，更偏向系统辅助功能。

### `remote`

用于封装遥控器和键鼠输入解析。

当前工程中的 `robot_cmd` 通过该模块获取遥控器拨杆、摇杆、键鼠和按键计数信息，因此它是云台控制输入侧的核心模块。

## 当前工程中的主要依赖关系

从当前保留代码看，真正构成云台控制主链的模块主要有：

- `algorithm`
- `daemon`
- `imu`
- `message_center`
- `motor`
- `remote`

它们在当前工程中的关系大致如下：

- `remote` 为 `robot_cmd` 提供输入数据
- `message_center` 为 `robot_cmd`、`gimbal`、`shoot` 提供模块间通信能力
- `imu` 为 `gimbal` 和姿态解算任务提供姿态与角速度数据
- `motor` 为 `gimbal` 和 `shoot` 提供执行机构控制能力
- `algorithm` 为 `imu` 和 `motor` 提供底层算法支持
- `daemon` 由独立任务周期执行，提供系统守护能力

`referee`、`alarm` 和 `ist8310` 当前更多属于保留模块，而不是主控制链的刚性依赖。

## 当前层级职责

`modules` 根目录这一层只承担两项职责：

- 组织模块目录
- 提供模块层总览说明

具体实现应由各自子目录维护，各子目录的 `.c/.h` 文件关系、输入输出和使用方式，应在对应子目录自己的说明文件中继续展开。

## 维护约束

维护 `modules` 根目录时，建议遵守以下约束：

- 根目录说明文件只做总览，不展开子目录内部细节
- 每个子目录都应有自己的独立说明文件
- 若模块仍被保留但不处于主控制链中，应在说明中明确其状态
- 若后续继续裁剪模块，应同步更新本文件中的子目录列表和模块定位说明
