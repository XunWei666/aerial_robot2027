# DMmotor 模块说明

## 基本信息

- 士继 DREAMER 战队
- 作者：Xun Wei
- 参考文献：湖南大学跃鹿战队相关开源代码与文档
- 本工程为 RoboMaster 无人机云台部分代码

## 文件夹作用

`DMmotor` 文件夹用于封装达妙电机在本工程中的控制逻辑。  
该目录当前保留了达妙电机的 CAN 通信、反馈解析、参考值设置和独立任务发送机制，但在本工程当前版本中并未接入任何实际应用模块。

换句话说，这个目录目前属于“保留驱动能力但未投入当前云台控制链使用”的模块。

## 当前目录文件说明

### `dmmotor.h`

头文件，定义了达妙电机模块的主要数据结构和对外接口。

其中主要内容包括：

- `DM_Motor_Measure_s`
  用于保存电机位置、速度、力矩和温度等反馈数据。
- `DMMotor_Send_s`
  用于组织达妙电机发送报文中的位置、速度、力矩、刚度和阻尼等字段。
- `DMMotorInstance`
  用于表示一个已注册的达妙电机实例，内部包含：
  - 电机反馈数据
  - 电机控制配置
  - PID 控制器
  - 反馈与前馈指针
  - 当前参考值
  - CAN 实例
  - 守护实例
- `DMMotorInit()`
  用于初始化并注册一个达妙电机实例。
- `DMMotorSetRef()`
  用于设置参考输入。
- `DMMotorOuterLoop()`
  用于切换最外层闭环类型。
- `DMMotorEnable()` 与 `DMMotorStop()`
  用于控制电机启停。
- `DMMotorCaliEncoder()`
  用于执行零位校准。
- `DMMotorControlInit()`
  用于为已注册电机创建控制任务。

### `dmmotor.c`

源文件，完成达妙电机模块的具体实现。

其核心逻辑包括：

- 在 `DMMotorInit()` 中创建电机实例、注册 CAN 回调和离线守护
- 在 `DMMotorDecode()` 中解析达妙电机反馈报文
- 在 `DMMotorTask()` 中周期生成控制报文并发送
- 在 `DMMotorControlInit()` 中为每个已注册达妙电机创建一个独立任务

这套实现和 `DJImotor`、`LKmotor` 的统一控制任务模式不同，它采用的是“每台电机单独创建任务”的方式。

## 在工程中的使用位置

根据当前工程代码搜索结果，`DMmotor` 模块目前没有被任何应用模块实际使用。

具体表现为：

- [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 未使用达妙电机接口
- [shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c) 未使用达妙电机接口
- [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h) 未调用 `DMMotorControlInit()`
- [motor_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/motor_task.c) 也未调度达妙电机控制逻辑

因此，在当前这套 RoboMaster 无人机云台工程中，`DMmotor` 只是保留驱动，不属于正在运行的控制链。

## 与其他模块的关系

`DMmotor` 模块主要依赖以下模块：

- [motor_def.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/motor_def.h)
  提供通用电机控制配置和控制相关枚举
- [controller.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/algorithm/controller.h)
  提供 PID 初始化和 PID 计算接口
- [bsp_can.h](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/can/bsp_can.h)
  提供 CAN 注册、发送与接收回调能力
- [daemon.h](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/daemon/daemon.h)
  提供掉线检测能力

同时，它内部还依赖 RTOS 任务接口来创建独立控制线程，因此它的调度方式和当前工程已使用的统一电机任务模式并不相同。

## 使用方式说明

如果后续需要在本工程中启用达妙电机，一般流程如下：

1. 在应用初始化阶段构造 `Motor_Init_Config_s`
2. 调用 `DMMotorInit()` 获取电机实例指针
3. 根据控制需求调用：
   - `DMMotorSetRef()` 设置目标值
   - `DMMotorOuterLoop()` 切换闭环类型
   - `DMMotorEnable()` 或 `DMMotorStop()` 控制启停
   - `DMMotorCaliEncoder()` 做零位校准
4. 在系统初始化中调用 `DMMotorControlInit()`，为已注册电机创建独立控制任务

需要注意的是，这套流程当前并未在本工程里启用。

## 设计说明

当前这版实现有两个值得注意的点。

### 当前只实现了力矩控制主路径

从 [dmmotor.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DMmotor/dmmotor.c) 中的实现可以看出，当前任务发送逻辑主要围绕力矩指令展开，位置和速度目标位基本固定为零。文件中也明确留下了后续扩展位置控制和更多 PID 模式的注释。

因此，这套实现目前更像一个“可继续扩展的驱动基础版本”，而不是已经在当前工程中成熟投入使用的控制模块。

### 调度方式与当前工程主链不同

`DJImotor` 和 `LKmotor` 当前都由 [motor_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/motor_task.c) 统一调度。  
而 `DMmotor` 则采用“每个电机一个任务”的方式。

这意味着如果后续要启用该模块，最好先评估是否继续沿用独立任务方案，还是改成和当前主链一致的统一调度方式。

## 备注

- 当前工程中该模块未被实际使用
- 当前目录缺少与应用层的直接连接代码
- 该目录的说明仅针对当前文件夹内的源码文件编写
