# daemon 模块说明

## 基本信息

- 士继 DREAMER 战队
- 作者：Xun Wei
- 参考文献：湖南大学跃鹿战队相关开源代码与文档
- 本工程为 RoboMaster 无人机云台部分代码

## 文件夹作用

`daemon` 文件夹用于提供统一的离线监测机制。  
它的作用不是直接控制某个硬件，而是给其他模块提供一种“定时递减计数，收到新数据后重新装填计数”的在线状态判断能力。

在当前工程中，`daemon` 主要用于这些对象的在线检测：

- 遥控器输入链路
- DJI 电机
- LK 电机
- DM 电机
- 裁判系统

因此，它本质上是一个被多个模块复用的基础支撑模块。

## 当前目录文件说明

### `daemon.h`

头文件，定义了离线监测模块的主要结构体和对外接口。

其中主要内容包括：

- `offline_callback`
  离线回调函数类型，当某个实例判定离线后调用。
- `DaemonInstance`
  表示一个守护实例，内部包含：
  - 重装计数值 `reload_count`
  - 当前剩余计数 `temp_count`
  - 离线回调函数
  - 所属对象指针 `owner_id`
- `Daemon_Init_Config_s`
  用于在注册时配置计数周期、初始化等待时间、离线回调和所属对象
- `DaemonRegister()`
  用于注册一个新的守护实例
- `DaemonReload()`
  用于在对象收到新数据或完成正常通信后重装计数
- `DaemonIsOnline()`
  用于判断某个实例当前是否在线
- `DaemonTask()`
  由系统任务周期调用，统一递减所有实例的计数并在超时后触发回调

### `daemon.c`

源文件，完成离线监测模块的具体实现。

其核心逻辑包括：

- 在 `DaemonRegister()` 中创建守护实例并保存到全局实例表
- 在 `DaemonReload()` 中重装剩余计数
- 在 `DaemonIsOnline()` 中根据剩余计数判断在线状态
- 在 `DaemonTask()` 中遍历所有已注册实例，统一做计数递减和超时处理

这套实现采用统一轮询调度，而不是每个模块自己维护超时计时器，因此工程中所有需要离线判断的模块都可以复用同一套机制。

## 在工程中的使用位置

当前工程中，`daemon` 模块被多个目录直接使用。

### 在系统任务中的使用

在 [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h) 中：

- `DaemonTask()` 被单独放入守护任务周期运行

这说明所有离线检测都依赖这一周期任务持续执行。  
如果 `DaemonTask()` 不运行，那么各模块虽然已经注册了守护实例，但不会发生计数递减，也就无法真正产生离线判定。

### 在遥控器模块中的使用

在 [remote_control.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/remote/remote_control.c) 中：

- 初始化时调用 `DaemonRegister()` 注册遥控器守护实例
- 每次成功接收 DBUS 数据后调用 `DaemonReload()`
- 上层通过 `DaemonIsOnline()` 判断遥控器是否在线

这说明遥控器模块并不自己实现离线计数逻辑，而是完全依赖 `daemon` 来判断通信是否超时。

### 在电机模块中的使用

在以下电机驱动中：

- [dji_motor.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DJImotor/dji_motor.c)
- [LK4005.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/LKmotor/LK4005.c)
- [dmmotor.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DMmotor/dmmotor.c)

这些模块都会：

- 在初始化时调用 `DaemonRegister()`
- 在成功收到反馈报文后调用 `DaemonReload()`
- 在超时后由离线回调执行日志或状态处理

这说明 `daemon` 也是当前电机在线检测的公共底座。

### 在裁判系统模块中的使用

在 [rm_referee.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/referee/rm_referee.c) 中：

- 裁判系统通信也通过 `DaemonRegister()` 和 `DaemonReload()` 来完成在线状态维护

虽然裁判系统当前不是主控制链核心，但它仍然使用了同一套守护逻辑。

## 与其他模块的关系

`daemon` 模块与当前工程的关系比较特殊：

- 它不直接产生控制目标
- 也不直接驱动外设
- 它主要负责给其他模块补充“是否还在线”这一状态维度

它当前主要服务于：

- [remote](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/remote/remote.md)
- [motor/DJImotor](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DJImotor/dji_motor.md)
- [motor/LKmotor](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/LKmotor/LK_motor.md)
- [motor/DMmotor](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DMmotor/dmmotor.md)
- [referee](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/referee)

而它自己的周期调度，又依赖 [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h) 中的守护任务。

## 使用方式说明

在当前工程中，使用 `daemon` 的一般流程如下：

1. 在模块初始化阶段构造 `Daemon_Init_Config_s`
2. 调用 `DaemonRegister()` 获取守护实例
3. 在模块每次成功收到新数据或完成有效通信后调用 `DaemonReload()`
4. 在需要判断在线状态的地方调用 `DaemonIsOnline()`
5. 由系统中的 `DaemonTask()` 周期递减所有实例计数并在超时后触发回调

这种模式适合所有“有周期性反馈或通信”的模块。

## 设计说明

当前这版实现最重要的设计点有两个。

### 统一管理所有在线检测对象

工程里并没有让每个模块自己维护倒计时变量，而是将所有需要检测在线状态的对象统一注册到 `daemon` 模块中，再由一个周期任务统一处理。

这样设计的好处是：

- 各模块实现方式统一
- 在线判断逻辑集中
- 离线回调入口统一

对当前这种包含多个电机、遥控器和通信模块的控制工程来说，这种集中式守护更容易维护。

### 通过 `owner_id` 回到具体实例

守护模块本身并不知道自己监测的是哪个模块类型，它只保存一个 `void *owner_id`。  
当离线发生时，回调函数再把这个指针转换回具体模块实例。

这样设计的理由是：

- `daemon` 不和任何具体模块绑定
- 同一个离线回调可以服务多个实例
- 保持模块间低耦合

这也是它能够同时服务电机、遥控器和裁判系统的原因。

## 备注

- 当前这版 `daemon` 是整个工程的公共基础模块
- 该目录的说明仅针对当前文件夹内的源码文件编写
- 当前代码中 `DaemonRegister()` 仍然存在初始化计数覆盖的问题，后续如果继续做基础设施修整，可以单独处理这一点
