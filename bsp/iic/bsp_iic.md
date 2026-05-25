# bsp_iic 模块说明

## 基本信息

- 士继 DREAMER 战队
- 作者：Xun Wei
- 参考文献：湖南大学跃鹿战队相关开源代码与文档
- 本工程为 RoboMaster 无人机云台部分代码

## 文件夹作用

`bsp/iic` 文件夹用于封装 STM32 的 I2C 外设访问接口。  
它为上层模块提供统一的：

- IIC 实例注册
- 主机发送与接收
- 寄存器读写访问
- 中断或 DMA 模式接收回调分发
- 序列传输控制

当前这套接口本身是通用的，但在本工程当前实际运行链中，它并不是主控制链核心接口。

## 当前目录文件说明

### `bsp_iic.h`

头文件，定义了 IIC 模块的宏、枚举、实例结构体、初始化结构体和对外接口。

主要内容包括：

- `IIC_DEVICE_CNT`
  当前板级可用 I2C 外设数量
- `MX_IIC_SLAVE_CNT`
  允许注册的从设备实例数量上限
- `IIC_Work_Mode_e`
  工作模式枚举，包括：
  - 阻塞模式
  - 中断模式
  - DMA 模式
- `IIC_Mem_Mode_e`
  寄存器访问模式枚举，包括读寄存器和写寄存器
- `IIC_Seq_Mode_e`
  序列传输模式枚举，用于决定本次访问后是否释放总线
- `IICInstance`
  表示一个已注册的 IIC 实例，内部包含：
  - I2C 句柄
  - 设备地址
  - 工作模式
  - 接收缓冲信息
  - 回调函数
  - 所属对象指针 `id`
- `IIC_Init_Config_s`
  用于注册 IIC 实例时传入初始化参数
- `IICRegister()`
  注册 IIC 实例
- `IICSetMode()`
  切换工作模式
- `IICTransmit()`
  主机发送数据
- `IICReceive()`
  主机接收数据
- `IICAccessMem()`
  对从设备寄存器进行阻塞式读写

### `bsp_iic.c`

源文件，完成 IIC 模块的具体实现。

其核心逻辑包括：

- 在 `IICRegister()` 中完成实例分配和注册
- 在 `IICTransmit()`、`IICReceive()` 中根据工作模式调用不同 HAL 接口
- 在 `IICAccessMem()` 中统一封装寄存器读写访问
- 在 `HAL_I2C_MasterRxCpltCallback()` 中根据句柄和设备地址找到对应实例，并触发其回调函数
- 在 `HAL_I2C_MemRxCpltCallback()` 中复用同样的接收完成处理逻辑

从接口设计上看，这个模块兼顾了：

- 直接主机收发
- 面向寄存器设备的内存访问

更适合传感器类器件使用。

## 在工程中的使用位置

### 在 IST8310 模块中的使用

在 [ist8310.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/ist8310/ist8310.c) 中：

- `IST8310Init()` 会调用 `IICRegister()`
- 通过 `IICAccessMem()` 完成寄存器初始化和磁场数据读取
- 在回调中解析接收到的磁力计数据

这说明 `bsp_iic` 当前在工程中最明确的上层使用者是 `ist8310` 模块。

### 在当前主运行链中的实际状态

虽然 `bsp_iic` 本身是完整可用的，但当前工程主运行链中的 IMU 实际使用的是：

- [BMI088driver.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/BMI088driver.c)
- [BMI088Middleware.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/imu/BMI088Middleware.c)

并且当前配置启用的是 `SPI` 路径，而不是 `IIC` 路径。

因此，在当前这套云台专用工程里：

- `bsp_iic` 能力保留
- 但不承担当前 IMU 主链工作

## 与其他模块的关系

`bsp_iic` 当前主要与以下目录存在直接关系：

- [ist8310](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/ist8310/ist8310.md)
  当前最明确的 IIC 设备使用者
- [gpio](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/gpio)
  某些 IIC 设备会配合外部中断引脚使用

从结构上讲，它也可以服务其他 IIC 传感器，但当前工程并没有进一步接入更多基于该接口的运行模块。

## 使用方式说明

在当前工程中，如果需要使用 `bsp_iic`，一般流程如下：

1. 构造 `IIC_Init_Config_s`
2. 填写：
   - `handle`
   - `dev_address`
   - `work_mode`
   - `callback`
   - `id`
3. 调用 `IICRegister()` 获取 IIC 实例
4. 根据需求选择：
   - `IICTransmit()` / `IICReceive()`
   - `IICAccessMem()`
5. 如果使用中断或 DMA 接收，在接收完成后由回调函数继续做数据解析

对传感器类设备来说，`IICAccessMem()` 往往是最直接的使用方式。

## 设计说明

当前这版 `bsp_iic` 有两个比较明显的设计特点。

### 同时支持普通传输和寄存器访问

很多 I2C 设备最终都需要读写内部寄存器，因此模块没有只提供基础发送接收接口，而是额外提供了 `IICAccessMem()`。

这样设计的好处是：

- 传感器驱动实现更直接
- 上层模块不需要自己重复封装寄存器访问逻辑

### 当前更偏保留基础能力

从当前工程的运行链来看，`bsp_iic` 并不是像 `bsp_can`、`bsp_usart` 那样的主路径底层模块。  
它当前更像是：

- 已保留的通用 I2C 外设能力
- 为 `ist8310` 这类备用传感器模块提供支持

而不是当前云台控制闭环直接依赖的关键接口。

## 备注

- 当前工程中 `bsp_iic` 不是主运行链核心接口
- 当前目录的说明仅针对当前文件夹内的源码文件编写
- 当前实现中仍有明显可继续整理的问题，例如 `IICRegister()` 中重复 `malloc`，后续如继续清理基础设施可以单独修正
