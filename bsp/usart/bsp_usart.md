# bsp_usart 模块说明

## 基本信息

- 士继 DREAMER 战队
- 作者：Xun Wei
- 参考文献：湖南大学跃鹿战队相关开源代码与文档
- 本工程为 RoboMaster 无人机云台部分代码

## 文件夹作用

`bsp/usart` 文件夹用于封装 STM32 串口外设访问接口。  
它为上层模块提供统一的：

- 串口实例注册
- 接收缓冲管理
- DMA 空闲中断接收
- 接收完成后的模块回调分发
- 发送接口封装

在当前工程中，`bsp_usart` 是遥控器输入链和裁判系统通信链的底层入口。

## 当前目录文件说明

### `bsp_usart.h`

头文件，定义了 USART 模块的宏、枚举、实例结构体、初始化结构体和对外接口。

主要内容包括：

- `DEVICE_USART_CNT`
  允许注册的串口实例数量上限
- `USART_RXBUFF_LIMIT`
  单个串口实例接收缓冲区上限
- `USART_TRANSFER_MODE`
  串口发送模式枚举，包括：
  - 阻塞发送
  - 中断发送
  - DMA 发送
- `USARTInstance`
  表示一个已注册的串口实例，内部包含：
  - 接收缓冲区
  - 接收包长度上限
  - 串口句柄
  - 模块接收回调函数
- `USART_Init_Config_s`
  用于注册串口实例时传入初始化参数
- `USARTRegister()`
  用于注册一个新的串口实例
- `USARTServiceInit()`
  用于启动或重启该实例的接收服务
- `USARTSend()`
  用于按指定模式发送数据
- `USARTIsReady()`
  用于判断当前串口是否处于可发送状态

### `bsp_usart.c`

源文件，完成 USART 模块的具体实现。

其核心逻辑包括：

- 在 `USARTRegister()` 中完成实例分配和注册
- 在 `USARTServiceInit()` 中启动 `ReceiveToIdle DMA` 接收
- 在 `HAL_UARTEx_RxEventCallback()` 中根据触发中断的串口句柄找到对应实例，并调用其模块回调函数
- 在 `HAL_UART_ErrorCallback()` 中处理串口错误并重新启动接收
- 在 `USARTSend()` 中统一封装阻塞、中断和 DMA 三种发送方式

从当前实现来看，这个模块的核心思想是：

- 接收统一交给 `DMA + IDLE`
- 协议解析不放在 `bsp` 层，而放到各模块自己的回调里

## 在工程中的使用位置

当前工程中，`bsp_usart` 主要服务于两个模块。

### 在遥控器模块中的使用

在 [remote_control.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/remote/remote_control.c) 中：

- `RemoteControlInit()` 会构造 `USART_Init_Config_s`
- 通过 `USARTRegister()` 注册遥控器串口实例
- 将 `RemoteControlRxCallback()` 作为接收回调
- 在掉线恢复时通过 `USARTServiceInit()` 重新启动接收

这说明遥控器数据链完全建立在 `bsp_usart` 的接收框架之上。

### 在裁判系统模块中的使用

在 [rm_referee.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/referee/rm_referee.c) 中：

- `RefereeInit()` 会通过 `USARTRegister()` 注册裁判系统串口实例
- 将 `RefereeRxCallback()` 作为接收回调
- 在离线恢复时通过 `USARTServiceInit()` 重新启动接收
- 发送裁判系统 UI 或交互数据时通过 `USARTSend()` 输出

这说明 `bsp_usart` 同时承担了裁判系统的数据接收与发送底层支持。

### 在应用层中的实际入口

在 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 中：

- `RemoteControlInit(&huart3)` 是当前实际的遥控器输入入口

这意味着 `application` 层虽然不直接调用 `bsp_usart`，但整个输入链实际上是从这里开始的。

## 与其他模块的关系

`bsp_usart` 当前主要服务于：

- [remote](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/remote/remote.md)
- [referee](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/referee/referee.md)

同时，它自身依赖 HAL 提供的串口 DMA 和空闲中断能力。

因此，在当前工程中，`bsp_usart` 的定位是：

- 串口类通信模块的统一底层入口
- 上层协议模块与 HAL 串口驱动之间的适配层

## 使用方式说明

在当前工程中，使用 `bsp_usart` 的一般流程如下：

1. 构造 `USART_Init_Config_s`
2. 填写：
   - `usart_handle`
   - `recv_buff_size`
   - `module_callback`
3. 调用 `USARTRegister()` 获取串口实例
4. 接收时由 `DMA + IDLE` 自动进入模块回调函数
5. 发送时调用 `USARTSend()`
6. 如果发生错误或需要重启接收，可调用 `USARTServiceInit()`

对上层模块来说，关键点在于：

- 选择正确的串口句柄
- 设定合适的接收包长度
- 提供自己的协议解析回调函数

## 设计说明

当前这版 `bsp_usart` 有几个对理解工程结构比较重要的设计点。

### 接收与协议解析分离

`bsp_usart` 只负责把一包数据收下来，并在接收完成时调用模块回调。  
至于这包数据是什么协议、怎么解包、如何更新模块状态，都交给上层模块自己处理。

这样设计的好处是：

- `bsp` 层保持通用
- 不同协议可以复用同一套接收框架
- 遥控器、裁判系统这类不同模块可以共享一个统一底层

### 使用 `ReceiveToIdle DMA`

当前实现采用的是 `HAL_UARTEx_ReceiveToIdle_DMA()`。  
这意味着它更适合处理“长度可变、以空闲间隔作为一包结束标志”的数据流。

对当前工程来说，这种模式非常适合：

- 遥控器数据接收
- 裁判系统串口数据接收

### 错误后自动重启接收

在 `HAL_UART_ErrorCallback()` 中，模块会重新启动接收。  
这说明设计目标不是仅仅发现错误，而是尽量恢复串口接收链路，减少人工干预。

## 备注

- 当前工程中 `bsp_usart` 是输入链和裁判系统通信链的关键底层模块
- 该目录的说明仅针对当前文件夹内的源码文件编写
- 当前实现仍保留了一些待优化点，例如连续 DMA/IT 发送时的发送队列问题
