# application

<p align='right'>士继 DREAMER 战队</p>
<p align='right'>作者：Xun Wei</p>
<p align='right'>参考文献：湖南大学跃鹿战队相关开源代码与文档</p>

## 说明

本目录是 **RoboMaster 无人机云台部分代码** 的应用层根目录。  
本说明文件仅介绍当前目录中的 `.c` 与 `.h` 文件，不展开子目录中的具体应用实现。

当前目录内需要说明的文件如下：

- `robot.c`
- `robot.h`
- `robot_def.h`
- `robot_task.h`

## 文件关系

### `robot.h`

该文件提供应用层总入口声明。

- `RobotInit()` 用于完成应用层初始化。
- `RobotTask()` 用于执行应用层核心周期任务。

该头文件的作用是把应用层入口统一暴露给上层，而不是让上层直接依赖各个具体应用模块。

### `robot.c`

该文件实现应用层初始化入口与总任务调度入口。

当前工程的初始化流程为：

1. 关闭中断。
2. 调用 `BSPInit()` 完成底层初始化。
3. 调用 `MessageCenterInit()` 初始化消息中心。
4. 依次初始化 `RobotCMD`、`Gimbal`、`Shoot` 应用。
5. 调用 `MessageCenterFreezeRegistry()` 冻结消息注册表。
6. 调用 `OSTaskInit()` 创建任务。
7. 重新开启中断。

`RobotTask()` 只调度当前工程实际启用的三个应用任务：

- `RobotCMDTask()`
- `GimbalTask()`
- `ShootTask()`

将这些流程集中在 `robot.c` 中，是为了让应用层保持单一入口，后续调整初始化顺序或裁剪应用时，不需要在多个位置分散修改。

### `robot_def.h`

该文件用于集中定义应用层共享的数据类型与关键机构参数。

当前内容主要分为两部分：

#### 1. 机构参数

主要包括：

- 云台 yaw / pitch 限位相关参数
- 拨弹盘单发角度
- 拨弹减速比
- 陀螺仪与云台坐标方向关系

这些参数会被 `robot_cmd`、`gimbal`、`shoot` 等应用共同使用，因此统一放在该文件中维护。

#### 2. 应用层消息类型

当前应用层共享的消息与状态类型主要包括：

- `System_State_e`
- `gimbal_mode_e`
- `friction_mode_e`
- `shoot_freq_e`
- `loader_mode_e`
- `Gimbal_Ctrl_Cmd_s`
- `Shoot_Ctrl_Cmd_s`
- `Gimbal_Upload_Data_s`
- `Shoot_Upload_Data_s`

这些定义用于应用之间通过消息中心交换控制命令与反馈信息。  
将它们集中放在 `robot_def.h` 中，可以避免消息格式分散在不同模块里，降低后续修改成本。

### `robot_task.h`

该文件负责 RTOS 任务创建与任务入口实现。

当前文件中定义并创建的主要任务包括：

- `StartINSTASK()`
- `StartMOTORTASK()`
- `StartDAEMONTASK()`
- `StartROBOTTASK()`

其中：

- `INS` 任务负责姿态解算
- `MOTOR` 任务负责统一电机控制
- `DAEMON` 任务负责守护检测
- `ROBOT` 任务负责应用层核心调度

`OSTaskInit()` 负责统一创建这些任务。  
这样设计的目的是把任务节拍、优先级和入口集中维护，避免任务配置散落在多个文件中。

## 当前目录职责

`application` 根目录只负责以下三件事：

- 提供应用层统一入口
- 定义应用层共享参数与消息类型
- 创建应用层相关任务

它不负责具体的云台控制算法和发射逻辑实现，这些内容由子目录中的应用模块分别完成。

## 维护约束

维护本目录中的文件时，建议遵守以下约束：

- 应用层共享消息类型统一放入 `robot_def.h`
- 应用层总初始化与总调度统一放入 `robot.c`
- 应用层周期任务统一在 `robot_task.h` 中注册与实现

## 备注

本说明文件只覆盖当前目录中的源码与头文件。  
`cmd`、`gimbal`、`shoot` 子目录的内容由各自目录下的说明文件单独介绍。
