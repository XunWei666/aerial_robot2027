# alarm 模块说明

## 基本信息

- 士继 DREAMER 战队
- 作者：Xun Wei
- 参考文献：湖南大学跃鹿战队相关开源代码与文档
- 本工程为 RoboMaster 无人机云台部分代码

## 文件夹作用

`alarm` 文件夹当前只包含蜂鸣器报警模块，用于在系统需要提示或报警时驱动板载蜂鸣器发声。

它的设计目标是：

- 支持多个报警源注册
- 通过报警等级决定优先输出哪一路报警
- 通过音调和响度控制蜂鸣器表现形式

不过在当前工程默认运行配置下，这个模块并未真正启用。

## 当前目录文件说明

### `buzzer.h`

头文件，定义了蜂鸣器模块的宏、枚举、配置结构体、实例结构体和对外接口。

主要内容包括：

- `BUZZER_DEVICE_CNT`
  允许注册的蜂鸣器报警源数量
- 音阶频率宏
  用于将不同音调映射到 PWM 周期
- `octave_e`
  音阶枚举
- `AlarmLevel_e`
  报警等级枚举
- `AlarmState_e`
  报警开关状态枚举
- `Buzzer_config_s`
  蜂鸣器报警源注册配置
- `BuzzzerInstance`
  蜂鸣器报警源实例
- `BuzzerInit()`
  初始化蜂鸣器 PWM 输出
- `BuzzerTask()`
  周期扫描报警源并输出蜂鸣器控制
- `BuzzerRegister()`
  注册一个新的报警源
- `AlarmSetStatus()`
  设置某个报警源的当前状态

### `buzzer.c`

源文件，完成蜂鸣器模块的具体实现。

其核心逻辑包括：

- 在 `BuzzerInit()` 中初始化 PWM 通道
- 在 `BuzzerRegister()` 中注册不同报警等级的报警源
- 在 `AlarmSetStatus()` 中控制某个报警源开关
- 在 `BuzzerTask()` 中遍历所有报警源，并按优先级决定当前蜂鸣器输出

从当前实现可以看出，这个模块采用的是“多报警源竞争一个蜂鸣器输出”的设计。

## 在工程中的使用位置

### 在任务层中的当前状态

在 [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h) 中：

- 仍然保留了 `#include "buzzer.h"`
- `BuzzerInit()` 调用被注释掉
- `BuzzerTask()` 调用也被注释掉

这说明当前工程虽然保留了蜂鸣器模块代码，但默认运行链并不会真正初始化和调度它。

### 在其他模块中的关联

在 [daemon.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/daemon/daemon.c) 中仍然保留了 `#include "buzzer.h"`，并在注释中提到后续可增加离线报警功能。

这说明蜂鸣器模块原本被考虑用于：

- 设备离线报警
- 系统状态提示

但当前这部分联动还未真正接入执行逻辑。

## 与其他模块的关系

`alarm` 模块当前主要依赖：

- [bsp_pwm.h](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/pwm/bsp_pwm.h)
  提供 PWM 注册、占空比设置和周期设置能力
- [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h)
  提供任务入口和周期调度位置

同时，它和 [daemon](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/daemon/daemon.md) 在设计上有潜在联动关系，因为设备离线事件很适合通过蜂鸣器告警。

## 使用方式说明

如果后续需要重新启用蜂鸣器模块，一般流程如下：

1. 在系统初始化阶段调用 `BuzzerInit()`
2. 为需要报警的模块构造 `Buzzer_config_s`
3. 调用 `BuzzerRegister()` 获取对应报警源实例
4. 在运行中通过 `AlarmSetStatus()` 控制报警开关
5. 在周期任务中调用 `BuzzerTask()` 扫描并输出蜂鸣器控制

当前工程只是保留了这套能力，但默认并没有启用这一流程。

## 设计说明

当前这版蜂鸣器模块有两个比较明确的设计点。

### 一个物理蜂鸣器对应多个报警源

代码中只维护了一个实际 PWM 蜂鸣器输出，但允许注册多个不同报警等级的逻辑报警源。  
这样设计的目的，是让不同模块都能申请报警能力，而底层只需要统一决定当前应该响哪一路。

### 当前更像预留能力，而不是已启用功能

从代码引用情况来看，蜂鸣器模块在当前工程里并没有真正接入任务链。  
因此它目前更适合作为：

- 预留的声音提示能力
- 后续可接入的告警模块

而不是当前固件运行所依赖的功能模块。

## 备注

- 当前目录只有蜂鸣器相关源码
- 当前工程默认未启用该模块
- 该目录的说明仅针对当前文件夹内的源码文件编写
