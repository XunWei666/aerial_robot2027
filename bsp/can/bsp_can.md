# bsp_can 模块说明

## 基本信息

- 士继 DREAMER 战队
- 作者：Xun Wei
- 参考文献：湖南大学跃鹿战队相关开源代码与文档
- 本工程为 RoboMaster 无人机云台部分代码

## 文件夹作用

`bsp/can` 文件夹用于封装 STM32 的 CAN 外设访问接口。  
它的目标不是直接完成某个电机或某个应用的控制，而是为上层模块提供统一的：

- CAN 实例注册
- 发送缓冲管理
- 接收过滤器配置
- 接收回调分发
- 报文发送接口

在当前工程中，`bsp_can` 是电机控制主链的关键底层模块。

## 当前目录文件说明

### `bsp_can.h`

头文件，定义了 CAN 模块的宏、实例结构体、初始化结构体和对外接口。

主要内容包括：

- `CAN_MX_REGISTER_CNT`
  允许注册的 CAN 实例数量上限
- `CANInstance`
  表示一个已注册的 CAN 实例，内部包含：
  - CAN 句柄
  - 发送报文头配置
  - 发送 ID
  - 发送邮箱号
  - 发送缓冲区
  - 接收缓冲区
  - 接收 ID
  - 接收长度
  - 接收回调函数
  - 所属对象指针 `id`
- `CAN_Init_Config_s`
  用于注册 CAN 实例时传入初始化参数
- `CANRegister()`
  用于注册一个新的 CAN 实例
- `CANSetDLC()`
  用于修改发送帧长度
- `CANTransmit()`
  用于发送当前实例缓冲区中的数据

### `bsp_can.c`

源文件，完成 CAN 模块的具体实现。

其核心逻辑包括：

- 在 `CANRegister()` 中完成实例分配、基础配置和过滤器注册
- 在 `CANServiceInit()` 中完成硬件 CAN 服务启动和中断使能
- 在 `CANAddFilter()` 中为每个实例配置接收过滤器
- 在 `CANTransmit()` 中等待邮箱空闲并完成报文发送
- 在 `CANFIFOxCallback()` 中从 FIFO 取出接收数据，并按 `can_handle + rx_id` 将消息分发给对应实例回调
- 通过 `HAL_CAN_RxFifo0MsgPendingCallback()` 与 `HAL_CAN_RxFifo1MsgPendingCallback()` 接入 HAL 中断回调

## 在工程中的使用位置

当前工程中，`bsp_can` 主要服务于电机驱动模块。

### 在 DJI 电机中的使用

在 [dji_motor.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DJImotor/dji_motor.c) 中：

- `DJIMotorInit()` 会调用 `CANRegister()` 为每个电机注册接收实例
- 电机反馈到达时，通过 `can_module_callback` 进入 `DecodeDJIMotor()`
- 控制输出通过 `CANTransmit()` 发送

这说明 DJI 电机模块依赖 `bsp_can` 同时完成反馈接收和控制报文发送。

### 在 LK 电机中的使用

在 [LK4005.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/LKmotor/LK4005.c) 中：

- `LKMotorInit()` 会调用 `CANRegister()`
- 电机反馈到达时进入 `LKMotorDecode()`
- 控制输出通过 `CANTransmit()` 发送

这说明云台俯仰轴对应的 LK 电机也直接建立在 `bsp_can` 之上。

### 在 DM 电机中的使用

在 [dmmotor.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DMmotor/dmmotor.c) 中：

- `DMMotorInit()` 会调用 `CANRegister()`
- 控制任务中通过 `CANTransmit()` 周期发报文

虽然 `DMmotor` 当前没有接入实际运行链，但底层仍然依赖 `bsp_can`。

### 在应用层中的总线分配

当前应用初始化代码中可以看到：

- [gimbal.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/gimbal/gimbal.c) 中云台偏航和俯仰电机都挂在 `hcan1`
- [shoot.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/shoot/shoot.c) 中发射机构电机挂在 `hcan2`

这说明当前工程已经在应用层完成了总线分工，而 `bsp_can` 负责将这种分工落到具体实例注册、过滤器配置和回调分发上。

## 与其他模块的关系

`bsp_can` 当前主要服务于：

- [DJImotor](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DJImotor/dji_motor.md)
- [LKmotor](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/LKmotor/LK_motor.md)
- [DMmotor](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/motor/DMmotor/dmmotor.md)

同时，它自身依赖：

- HAL 提供的 CAN 硬件驱动接口
- [dwt](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/dwt)
  用于发送超时等待
- [log](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/log)
  用于错误与警告输出

因此，`bsp_can` 在整套云台工程中处于“底层外设封装”和“电机驱动入口”之间的关键位置。

## 使用方式说明

在当前工程中，使用 `bsp_can` 的一般流程如下：

1. 构造 `CAN_Init_Config_s`
2. 填写：
   - `can_handle`
   - `tx_id`
   - `rx_id`
   - `can_module_callback`
   - `id`
3. 调用 `CANRegister()` 获取 CAN 实例
4. 发送时先写入 `tx_buff`
5. 调用 `CANTransmit()` 发送
6. 接收时由中断自动进入对应实例回调

对上层模块来说，最关键的是：

- 正确设置收发 ID
- 提供自己的解包回调函数
- 保存注册后返回的实例指针

## 设计说明

当前这版 `bsp_can` 有几个对理解工程结构比较重要的设计点。

### 以“实例”为单位管理 CAN 通信

这里不是提供一组全局的“直接发 CAN1”“直接收 CAN2”接口，而是让每个上层模块先注册自己的 `CANInstance`。

这样设计的好处是：

- 同一个 CAN 外设上可以挂多个逻辑设备
- 每个设备都拥有自己的收发 ID、缓冲区和回调函数
- 上层模块只关心自己的实例，不需要自己处理总线级细节

### 接收回调按 `can_handle + rx_id` 分发

中断发生后，`bsp_can` 会遍历已注册实例，找到：

- 来自同一条 CAN 总线
- 且接收 ID 匹配

的实例，再调用它的回调函数。

这意味着：

- `bsp_can` 只负责总线层路由
- 协议解析由具体模块自己完成

这种分工使 `DJImotor`、`LKmotor`、`DMmotor` 可以共用一套底层收发框架，而不用互相耦合。

### 当前工程显式使用双 CAN 分流

从应用层初始化可以看到，当前工程已经把：

- 云台电机放在 `CAN1`
- 发射机构电机放在 `CAN2`

因此 `bsp_can` 在当前工程里不只是一个通用驱动，它也实际承担了总线负载分流之后的底层实现角色。

## 备注

- 当前工程的电机主链高度依赖该目录
- 该目录的说明仅针对当前文件夹内的源码文件编写
- 当前实现里仍有一些注释提到历史扩展场景，但不影响本工程现有使用方式
