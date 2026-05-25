# referee 模块说明

## 基本信息

- 士继 DREAMER 战队
- 作者：Xun Wei
- 参考文献：湖南大学跃鹿战队相关开源代码与文档
- 本工程为 RoboMaster 无人机云台部分代码

## 文件夹作用

`referee` 文件夹用于封装 RoboMaster 裁判系统相关功能。  
当前目录同时包含三类内容：

- 裁判系统通信接收与发送
- 裁判系统协议定义与 CRC 校验
- 客户端 UI 绘制与刷新

在当前工程中，这一目录仍然保留完整能力，但它不属于云台与发射控制主链核心模块。

## 当前目录文件说明

### `rm_referee.c` / `rm_referee.h`

用于实现裁判系统串口通信、报文解析和数据管理。

主要职责包括：

- 初始化裁判系统串口
- 接收并解析裁判系统下发的数据帧
- 将解析结果写入 `referee_info_t`
- 提供 `RefereeInit()` 和 `RefereeSend()` 接口
- 通过 `daemon` 进行裁判系统在线监测

其中 `referee_info_t` 用于保存比赛状态、机器人状态、功率热量、位置、伤害、射击和交互数据等裁判系统反馈信息。

### `referee_protocol.h`

用于定义裁判系统协议中的命令号、帧格式、数据长度和数据结构。  
该文件本质上是裁判系统协议描述头文件，是整个 `referee` 目录的数据基础。

### `crc_ref.c` / `crc_ref.h`

用于实现裁判系统协议需要的 CRC8 和 CRC16 校验。  
它们主要服务于裁判系统数据帧收发，与 `algorithm` 目录中的通用 CRC 文件属于不同用途的实现。

### `referee_UI.c` / `referee_UI.h`

用于实现客户端图形绘制和刷新接口。

当前文件中提供了多种 UI 图元的绘制和发送能力，例如：

- 线段
- 矩形
- 圆
- 浮点数
- 字符串

这些接口本质上是对裁判系统客户端 UI 协议的二次封装。

### `referee_task.c` / `referee_task.h`

用于实现基于裁判系统客户端的 UI 初始化与刷新逻辑。

当前这部分代码主要完成：

- 机器人 ID 判定
- 初始 UI 图形绘制
- 模式变化检测
- 根据模式状态刷新 UI 显示

从当前实现看，这里的 UI 更偏向展示机器人模式与发射状态。

## 在工程中的使用位置

### 在任务层中的使用状态

在 [robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h) 中：

- 仍然保留了 `#include "referee_task.h"`
- 仍然保留了 `StartUITASK()` 的任务实现

但同时可以看到：

- UI 任务创建代码当前是注释状态

这说明当前工程中，`referee` 模块的 UI 刷新链并没有真正进入运行态。

换句话说：

- 代码能力保留
- 接口入口保留
- 当前默认运行链未启用

### 在应用层中的实际关系

当前 `robot_cmd`、`gimbal`、`shoot` 这条主控制链并不依赖 `referee` 才能运行。  
也就是说，即便不启用裁判系统 UI，当前云台与发射控制逻辑仍可独立工作。

因此，`referee` 在当前工程中的地位更准确地说是：

- 保留的通信与显示辅助模块
- 非主控制链必需模块

## 与其他模块的关系

`referee` 模块当前主要依赖以下模块：

- [bsp/usart](/D:/aerial_robot/aerial_robot2.0_oncodex/bsp/usart/bsp_usart.md)
  用于裁判系统串口收发
- [daemon](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/daemon/daemon.md)
  用于裁判系统在线状态检测
- [application/robot_task.h](/D:/aerial_robot/aerial_robot2.0_oncodex/application/robot_task.h)
  用于任务接入和调度

同时，`referee_task` 中的 UI 刷新逻辑还依赖：

- `robot_def.h` 中的模式枚举和交互数据结构

因此这个目录和应用层是有关联的，但这种关联主要体现在“状态显示”，而不是“核心控制”。

## 使用方式说明

如果在当前工程中重新启用裁判系统功能，一般流程如下：

1. 在初始化阶段调用 `RefereeInit()` 或 `UITaskInit()`
2. 配置并启动对应的 UI 任务
3. 在运行中由串口回调持续接收裁判系统数据
4. 在 UI 任务中根据模式变化刷新客户端显示

其中：

- 如果只需要接收裁判系统数据，可以只使用 `rm_referee`
- 如果还需要客户端 UI，则需要再启用 `referee_UI` 和 `referee_task`

## 设计说明

当前这版 `referee` 目录最需要明确的是它在工程中的“定位”。

### 功能完整，但当前不是主链

从代码完整度看，裁判系统通信解析和客户端 UI 能力都保留得比较完整。  
但从当前运行链看，它并没有参与云台控制和发射控制的关键闭环。

因此，在这套工程里它更适合被理解为：

- 保留的比赛辅助模块
- 可按需要启用的外围能力

而不是当前固件运行不可缺少的核心模块。

### 当前 UI 内容仍带有历史模板痕迹

从 [referee_task.c](/D:/aerial_robot/aerial_robot2.0_oncodex/modules/referee/referee_task.c) 可以看到：

- 仍保留 `"chassis:"` 等历史 UI 文本
- 也保留了部分底盘功率条相关绘制代码和注释

这说明该目录的 UI 层实现仍带有旧工程模板痕迹。  
如果后续要正式启用裁判系统 UI，建议再按当前云台专用工程的实际需求做一次专项精简。

## 备注

- 当前工程中 `referee` 模块默认未进入主运行链
- 当前目录仍保留较完整的裁判系统通信与 UI 能力
- 该目录的说明仅针对当前文件夹内的源码文件编写
