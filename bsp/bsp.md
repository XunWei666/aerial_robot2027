# bsp 层说明

## 基本信息

- 士继 DREAMER 战队
- 作者：Xun Wei
- 参考文献：湖南大学跃鹿战队相关开源代码与文档
- 本工程为 RoboMaster 无人机云台部分代码

## 文件夹作用

`bsp` 层用于封装单片机底层外设与片上基础能力。  
这一层不直接承担机器人业务逻辑，而是为 `modules` 层和 `application` 层提供统一、可复用的硬件访问接口。

在当前工程中，`bsp` 的主要职责包括：

- 对 CAN、USART、SPI、IIC、PWM、GPIO、USB 等外设做统一封装
- 提供日志、时间基准、工具函数等基础支持
- 通过实例注册方式，把底层外设能力交给上层模块使用

## 当前目录文件说明

### `bsp_init.h`

提供 `BSPInit()`，作为 `bsp` 层统一初始化入口。

从当前实现来看，`BSPInit()` 只初始化系统中最基础、最公共的底层能力：

- `DWT_Init(168)`
- `BSPLogInit()`

这说明当前工程并不在这里一次性初始化所有外设，而是只初始化运行全局都需要的底层组件。

### `bsp_tools.c` / `bsp_tools.h`

用于提供 `bsp` 层通用辅助工具。  
这类文件不直接对应某一个硬件外设，而是给 `bsp` 自身或更上层的封装提供公共支撑。

### 各子目录

当前 `bsp` 根目录下保留的外设子目录包括：

- [adc](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/adc)
- [can](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/can)
- [dwt](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/dwt)
- [flash](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/flash)
- [gpio](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/gpio)
- [iic](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/iic)
- [log](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/log)
- [pwm](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/pwm)
- [spi](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/spi)
- [usart](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/usart)
- [usb](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/usb)

这些目录分别封装不同外设能力，后续应逐个目录单独说明。

## 在工程中的使用位置

### 在系统启动中的使用

在 [robot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot.c) 中：

- `RobotInit()` 会首先调用 `BSPInit()`

这说明 `bsp` 层是整套工程启动链的最底层起点之一。  
系统先完成基础底层能力初始化，然后才会继续进入消息中心、应用模块和任务初始化。

### 在模块层中的使用

当前多个 `modules` 目录都直接依赖 `bsp` 层外设封装。

例如：

- [remote_control.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/remote/remote_control.c)
  依赖 `bsp_usart`
- [dji_motor.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DJImotor/dji_motor.c)
  依赖 `bsp_can`、`bsp_dwt`、`bsp_log`
- [LK4005.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/LKmotor/LK4005.c)
  依赖 `bsp_can`、`bsp_dwt`、`bsp_log`
- [rm_referee.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/referee/rm_referee.c)
  依赖 `bsp_usart`
- [ist8310.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/ist8310/ist8310.c)
  依赖 `bsp_iic`、`bsp_gpio`、`bsp_dwt`、`bsp_log`

这说明 `bsp` 层并不直接给 `application` 提供高层控制接口，而主要服务于 `modules` 层。

## 与其他层的关系

当前工程的层级关系可以概括为：

- `bsp`
  封装底层外设和基础能力
- `modules`
  基于 `bsp` 实现设备驱动、算法、消息通信和辅助功能
- `application`
  基于 `modules` 完成云台与发射机构控制逻辑

因此，`bsp` 在整个工程中的定位是：

- 最底层的硬件适配与基础封装层
- 为上层所有模块提供统一接口

## 使用方式说明

当前工程对 `bsp` 层的使用方式主要有两种：

1. 系统启动时通过 `BSPInit()` 初始化最基础能力
2. 模块初始化时按需调用某个外设目录下的注册接口，生成对应实例

也就是说：

- 不需要的外设，不会自动初始化
- 只有真正注册实例的外设，才会在运行中被使用

这种方式更适合当前这种云台专用工程，因为它能减少不必要的底层初始化和资源占用。

## 设计说明

当前这版 `bsp` 层有两个比较明确的设计特点。

### 最小化统一初始化

`BSPInit()` 并没有把所有外设都一次性拉起，而是只初始化公共底层能力。  
其余外设通过模块注册时按需初始化。

这样设计的好处是：

- 避免无用外设提前启动
- 减少初始化阶段耦合
- 更利于工程裁剪

这和当前工程已经被裁成“无人机云台专用板”是匹配的。

### 以实例注册为主

从 `can`、`usart`、`iic`、`pwm` 等目录的使用方式可以看出，当前 `bsp` 层强调实例注册而不是纯静态全局接口。

这样做可以：

- 同一类外设支持多个实例
- 上层模块只关心自己持有的实例句柄
- 不同模块之间更容易解耦

## 备注

- 当前 `bsp` 层仍保留多个外设目录，即使某些目录在当前云台工程中使用较少
- 该目录的说明仅针对 `bsp` 根目录内文件以及其组织关系编写
- 后续应继续为每个子目录分别补充或重写对应的目录说明文件
