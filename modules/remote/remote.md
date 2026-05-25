# remote

<p align='right'>士继 DREAMER 战队</p>
<p align='right'>作者：Xun Wei</p>
<p align='right'>参考文献：湖南大学跃鹿战队相关开源代码与文档</p>

## 说明

本目录是 **RoboMaster 无人机云台部分代码** 中的遥控器输入模块目录。

当前目录只包含两个需要说明的源码文件：

- `remote_control.h`
- `remote_control.c`

该模块的职责不是直接做云台或发射控制决策，而是把底层 DBUS 遥控器数据流解析成应用层可以直接读取的统一输入结构。当前工程中，`robot_cmd` 正是通过本模块获取遥控器拨杆、摇杆、键鼠和按键计数信息。

## 在工程中的位置

本模块在当前工程中的调用关系如下：

- 在 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 中，由 `RobotCMDInit()` 调用 `RemoteControlInit(&huart3)`
- 返回的 `RC_ctrl_t *` 指针被 `robot_cmd` 长期保存，用于周期读取输入状态
- 拨杆状态通过 `switch_is_down()`、`switch_is_mid()`、`switch_is_up()` 等宏在 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 中直接使用
- 键鼠输入通过 `mouse` 和 `key_count` 被 `robot_cmd` 解释为模式切换、摩擦轮开关和发射模式切换
- 底层依赖 `bsp_usart` 完成串口接收
- 在线状态检测依赖 `daemon` 模块完成

因此，本模块属于当前工程的输入解析层，是 `robot_cmd` 的主要输入来源之一。

## 当前目录文件

### `remote_control.h`

该文件定义了遥控器模块的对外数据结构、辅助宏和对外接口。

主要内容包括：

- 双缓冲索引宏 `TEMP` 和 `LAST`
- 按键统计索引宏 `KEY_PRESS`、`KEY_PRESS_WITH_CTRL`、`KEY_PRESS_WITH_SHIFT`
- 遥控器通道和拨杆状态宏
- 键盘键位索引定义
- 输入数据结构 `Key_t`
- 输入总结构 `RC_ctrl_t`
- 对外接口 `RemoteControlInit()` 和 `RemoteControlIsOnline()`

其中，`RC_ctrl_t` 是当前模块最核心的对外数据结构。它统一封装了：

- 遥控器摇杆和拨杆
- 侧边拨轮
- 鼠标移动和左右键
- 键盘状态
- 按键触发计数

应用层并不需要自己处理原始串口数据，而是直接读取这个结构体。

### `remote_control.c`

该文件实现遥控器串口注册、SBUS 数据解析、按键统计和离线检测逻辑。

主要负责以下工作：

1. 注册遥控器串口接收
2. 在接收回调中解析一帧遥控器数据
3. 把解析结果写入 `RC_ctrl_t`
4. 维护当前帧和上一帧数据
5. 统计普通按键、Ctrl 组合键和 Shift 组合键的触发次数
6. 注册守护对象并检测遥控器是否离线

## 核心数据设计

### 双缓冲

当前实现使用：

- `rc_ctrl[TEMP]`
- `rc_ctrl[LAST]`

作为输入双缓冲。

其作用是：

- 保存当前帧解析结果
- 保存上一帧解析结果
- 支持比较前后两帧按键状态
- 支持实现“按下一次切换一次”的逻辑

这也是 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 能够通过 `key_count` 做模式切换的基础。

### 统一输入结构

本模块的核心设计是把遥控器、鼠标和键盘都统一收拢到 `RC_ctrl_t` 中。

这样做的结果是：

- 上层不需要分别处理多种输入源
- 遥控器模式和键鼠模式可以复用同一份输入对象
- `robot_cmd` 可以在同一个任务里完成不同输入源的控制决策

### 按键计数

模块内部不仅保留了按键当前状态，还维护了：

- 普通按键触发次数
- Ctrl 组合键触发次数
- Shift 组合键触发次数

这样设计的原因是当前应用层存在大量“按一次切换一次”的逻辑，例如：

- `G` 键切换云台控制模式
- `E` 键切换发射模式
- `F` 键切换摩擦轮开关

如果只读取当前按下状态，上层就需要自行处理长按去抖和边沿判断，而这里直接由输入模块统一完成会更稳定。

## 主要内部逻辑

### `RectifyRCjoystick()`

该函数用于对摇杆输入做简单矫正。

当前策略是：

- 若某个摇杆或拨轮数值绝对值超过合理范围，则将其归零

它的作用是过滤异常值，避免上层直接消费明显失真的输入数据。

### `sbus_to_rc()`

这是本模块最核心的解析函数。

它主要完成：

- 从原始接收缓冲区解析四个摇杆通道
- 解析拨轮
- 解析左右拨杆状态
- 解析鼠标移动和左右按键
- 解析键盘位域
- 根据前后两帧数据统计按键触发次数
- 更新 `LAST` 缓冲区

该函数的输出就是当前上层实际读取的 `rc_ctrl[TEMP]`。

### `RemoteControlRxCallback()`

该函数作为串口接收回调使用。

当前执行顺序是：

1. 调用 `DaemonReload()` 刷新在线计数
2. 调用 `sbus_to_rc()` 解析接收缓冲区

这样设计的原因是：

- 只要持续收到遥控器数据，就说明遥控器在线
- 每次接收成功时顺便刷新守护计数最直接

### `RCLostCallback()`

该函数是遥控器离线回调。

当前离线后会：

- 清空遥控器数据
- 尝试重新初始化串口服务
- 输出告警日志

其目的在于：

- 防止上层继续使用旧输入数据
- 尝试恢复接收链路

## 对外使用方式

### 初始化阶段

在 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 中，`RobotCMDInit()` 会调用：

- `RemoteControlInit(&huart3)`

返回值保存为：

- `RC_ctrl_t *rc_data`

之后 `robot_cmd` 在运行阶段持续读取这份数据。

### 在 `robot_cmd` 中的典型用法

当前 `robot_cmd` 中对本模块的使用主要包括：

- 在 `ModeSwitch()` 中读取左右拨杆状态切换系统状态
- 在 `RemoteControl()` 中读取摇杆、拨轮和拨杆状态生成云台与发射控制命令
- 在 `MouseKeyControl()` 中读取鼠标和按键计数生成键鼠控制命令

也就是说，本模块负责提供输入数据，但不负责解释这些输入应该映射成什么样的机器人行为。

## 在线检测

本模块在 `RemoteControlInit()` 中会注册一个守护对象，用于检测遥控器是否在线。

当前逻辑是：

- 每次收到一帧遥控器数据时调用 `DaemonReload()`
- 若超过设定时间未收到数据，则认为遥控器离线
- 离线后触发 `RCLostCallback()`

当前对外提供：

- `RemoteControlIsOnline()`

用于让上层查询遥控器在线状态。

虽然 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 中对应的离线兜底逻辑目前保留为注释，但本模块本身已经具备在线检测能力。

## 当前目录职责

`remote` 在当前工程中的职责是：

- 解析遥控器输入
- 解析鼠标与键盘输入
- 提供统一输入结构
- 提供按键边沿计数能力
- 提供遥控器在线检测入口

它不负责控制策略决策，也不负责电机执行。

## 维护约束

维护本目录时，建议遵守以下约束：

- 原始串口协议解析统一维护在 `remote_control.c`
- 上层业务不要直接操作原始字节流，只读取 `RC_ctrl_t`
- 通道宏、拨杆状态宏和键位索引统一维护在 `remote_control.h`
- 若修改输入映射，应同步检查 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c) 的使用逻辑
- 不要在本目录中加入具体云台或发射的业务决策

## 备注

本说明文件只覆盖当前目录中的 `.c/.h` 文件。

文中提到 [robot_cmd.c](/D:/aerial_robot/aerial_robot2.0_oncodex/application/cmd/robot_cmd.c)、`bsp_usart` 和 `daemon` 仅用于说明本模块的调用位置、依赖来源和使用方式，不替代对应目录自己的说明文件。
