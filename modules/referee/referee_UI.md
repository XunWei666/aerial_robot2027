# 裁判系统 UI 绘制说明

本文档整理 `modules/referee` 目录下当前代码支持的 RoboMaster 客户端 UI 绘制规则、交互命令和封装接口，便于后续新增或修改 UI。

## 相关文件

- `referee_protocol.h`：裁判系统协议、交互数据 ID、图形数据结构、颜色/图形/操作枚举。
- `referee_UI.h`：UI 绘制和刷新接口声明。
- `referee_UI.c`：UI 图元封装、删除、刷新发送实现。
- `referee_task.c`：当前工程中的 UI 初始化和动态刷新示例。
- `rm_referee.h` / `rm_referee.c`：裁判系统串口初始化、接收解析和 `RefereeSend()` 发送接口。

## 基本发送流程

客户端 UI 绘制走裁判系统学生机器人交互通道：

1. 初始化裁判系统串口：调用 `UITaskInit()` 或 `RefereeInit()`。
2. 等待收到 `GameRobotState.robot_id`，确认本机器人 ID。
3. 计算客户端 ID：`Cilent_ID = 0x0100 + Robot_ID`。
4. 可选：调用 `UIDelete(..., UI_Data_Del_ALL, 0)` 清空旧 UI。
5. 调用具体绘制函数填充 `Graph_Data_t` 或 `String_Data_t`。
6. 调用 `UIGraphRefresh()` 或 `UICharRefresh()` 将绘制结果发送到客户端。
7. 后续刷新已有图形时，保持同一个 `graphic_name`，把操作类型改为 `UI_Graph_Change`。

当前 `referee_task.c` 中的参考流程：

```c
UITaskInit(&huart6, &ui_data);
MyUIInit();

while (1)
{
    UITask();
}
```

## 裁判系统帧格式

UI 和机器人间通信使用统一命令码：

| 字段 | 长度 | 说明 |
| --- | --- | --- |
| 帧头 `xFrameHeader` | 5 字节 | SOF、数据长度、包序号、CRC8 |
| `CmdID` | 2 字节 | UI 使用 `ID_student_interactive = 0x0301` |
| 交互数据头 | 6 字节 | `data_cmd_id`、发送者 ID、接收者 ID |
| 数据段 | 变长 | 删除命令、图形数据或字符串数据 |
| 帧尾 CRC16 | 2 字节 | 整帧 CRC16 |

帧头固定起始字节：

```c
#define REFEREE_SOF 0xA5
```

包序号由全局变量 `UI_Seq` 维护，部分发送函数会在发送后自增。

## 交互数据头

```c
typedef struct
{
    uint16_t data_cmd_id;
    uint16_t sender_ID;
    uint16_t receiver_ID;
} ext_student_interactive_header_data_t;
```

字段说明：

- `data_cmd_id`：交互内容 ID，决定本次发送是删除、绘图、绘制字符还是自定义交互。
- `sender_ID`：本机器人 ID，即 `referee_id.Robot_ID`。
- `receiver_ID`：客户端 ID，即 `referee_id.Cilent_ID`。

## UI 交互命令 ID

定义位置：`referee_protocol.h` 中的 `Interactive_Data_ID_e`。

| 命令 | 数值 | 数据内容 | 说明 |
| --- | --- | --- | --- |
| `UI_Data_ID_Del` | `0x100` | 删除操作数据 | 删除图层或全部 UI |
| `UI_Data_ID_Draw1` | `0x101` | 1 个图形 | 一次刷新 1 个 `Graph_Data_t` |
| `UI_Data_ID_Draw2` | `0x102` | 2 个图形 | 一次刷新 2 个 `Graph_Data_t` |
| `UI_Data_ID_Draw5` | `0x103` | 5 个图形 | 一次刷新 5 个 `Graph_Data_t` |
| `UI_Data_ID_Draw7` | `0x104` | 7 个图形 | 一次刷新 7 个 `Graph_Data_t` |
| `UI_Data_ID_DrawChar` | `0x110` | 1 个字符串图形 | 一次刷新 1 个 `String_Data_t` |
| `Communicate_Data_ID` | `0x0200` | 自定义交互数据 | 机器人间通信，不属于 UI 绘制 |

注意：`UIGraphRefresh()` 当前只处理 `cnt = 1、2、5、7`，不要传入其他数量。

## 数据长度

定义位置：`referee_protocol.h` 中的 `Interactive_Data_Length_e`。

| 宏/枚举 | 数值 | 说明 |
| --- | --- | --- |
| `Interactive_Data_LEN_Head` | 6 | 交互数据头长度 |
| `UI_Operate_LEN_Del` | 2 | 删除操作数据长度 |
| `UI_Operate_LEN_PerDraw` | 15 | 单个图形数据长度 |
| `UI_Operate_LEN_DrawChar` | 45 | 字符串绘制数据长度，15 字节图形控制 + 30 字节字符串 |

## 图形数据结构

```c
typedef struct
{
    uint8_t graphic_name[3];
    uint32_t operate_tpye : 3;
    uint32_t graphic_tpye : 3;
    uint32_t layer : 4;
    uint32_t color : 4;
    uint32_t start_angle : 9;
    uint32_t end_angle : 9;
    uint32_t width : 10;
    uint32_t start_x : 11;
    uint32_t start_y : 11;
    uint32_t radius : 10;
    uint32_t end_x : 11;
    uint32_t end_y : 11;
} Graph_Data_t;
```

### 通用字段规则

| 字段 | 说明 |
| --- | --- |
| `graphic_name[3]` | 图形名称，3 字节，用于唯一标识图形；修改或删除已有图形时必须保持同名。 |
| `operate_tpye` | 图形操作类型：新增、修改、删除。 |
| `graphic_tpye` | 图形类型：直线、矩形、圆、椭圆、圆弧、浮点数、整数、字符。 |
| `layer` | 图层，取值通常为 0~9。 |
| `color` | 图形颜色。 |
| `width` | 线宽；对字符/数字类也作为笔画宽度。 |
| `start_x` / `start_y` | 起点、圆心或文字起始坐标。 |
| `end_x` / `end_y` | 终点、对角点、半轴长度或数字编码字段。 |
| `radius` | 圆半径，或数字编码字段。 |
| `start_angle` / `end_angle` | 圆弧角度，或字符字号/字符串长度，或浮点小数位数。 |

代码中 `graphic_name` 的写入方式是反向填充：

```c
graph->graphic_name[2 - i] = graphname[i];
```

使用时仍按正常字符串传入 3 个以内字符，例如 `"sl0"`、`"sd1"`。

## 图形操作类型

定义位置：`UI_Graph_Operate_e`。

| 操作 | 数值 | 说明 |
| --- | --- | --- |
| `UI_Graph_ADD` | 1 | 新增图形。首次绘制某个 `graphic_name` 时使用。 |
| `UI_Graph_Change` | 2 | 修改图形。刷新已有 `graphic_name` 时使用。 |
| `UI_Graph_Del` | 3 | 删除图形。通常更常用 `UIDelete()` 删除图层或全部 UI。 |

规则：

- 同一个图形第一次显示用 `UI_Graph_ADD`。
- 后续动态刷新同一个图形用 `UI_Graph_Change`。
- `UI_Graph_Change` 需要复用相同的 `graphic_name`，否则客户端找不到原图形。

## 图形类型

定义位置：`UI_Graph_Type_e`。

| 类型 | 数值 | 封装函数 | 说明 |
| --- | --- | --- | --- |
| `UI_Graph_Line` | 0 | `UILineDraw()` | 直线 |
| `UI_Graph_Rectangle` | 1 | `UIRectangleDraw()` | 矩形 |
| `UI_Graph_Circle` | 2 | `UICircleDraw()` | 整圆 |
| `UI_Graph_Ellipse` | 3 | `UIOvalDraw()` | 椭圆 |
| `UI_Graph_Arc` | 4 | `UIArcDraw()` | 圆弧 |
| `UI_Graph_Float` | 5 | `UIFloatDraw()` | 浮点数 |
| `UI_Graph_Int` | 6 | `UIIntDraw()` | 整数 |
| `UI_Graph_Char` | 7 | `UICharDraw()` | 字符串 |

## 图形颜色

定义位置：`UI_Graph_Color_e`。

| 颜色 | 数值 | 说明 |
| --- | --- | --- |
| `UI_Color_Main` | 0 | 红蓝主色 |
| `UI_Color_Yellow` | 1 | 黄色 |
| `UI_Color_Green` | 2 | 绿色 |
| `UI_Color_Orange` | 3 | 橙色 |
| `UI_Color_Purplish_red` | 4 | 紫红色 |
| `UI_Color_Pink` | 5 | 粉色 |
| `UI_Color_Cyan` | 6 | 青色 |
| `UI_Color_Black` | 7 | 黑色 |
| `UI_Color_White` | 8 | 白色 |

## 删除 UI

接口：

```c
void UIDelete(referee_id_t *_id, uint8_t Del_Operate, uint8_t Del_Layer);
```

删除操作定义：

| 操作 | 数值 | 说明 |
| --- | --- | --- |
| `UI_Data_Del_NoOperate` | 0 | 不操作 |
| `UI_Data_Del_Layer` | 1 | 删除指定图层 |
| `UI_Data_Del_ALL` | 2 | 删除全部图层 |

示例：

```c
UIDelete(&referee_recv_info->referee_id, UI_Data_Del_ALL, 0);
UIDelete(&referee_recv_info->referee_id, UI_Data_Del_Layer, 7);
```

## 图形绘制接口

这些函数只负责填充图形数据，不会立刻发送；必须再调用刷新接口。

### 直线

```c
void UILineDraw(Graph_Data_t *graph, char graphname[3], uint32_t Graph_Operate,
                uint32_t Graph_Layer, uint32_t Graph_Color, uint32_t Graph_Width,
                uint32_t Start_x, uint32_t Start_y, uint32_t End_x, uint32_t End_y);
```

字段含义：

- `(Start_x, Start_y)`：起点。
- `(End_x, End_y)`：终点。

示例：

```c
Graph_Data_t line;
UILineDraw(&line, "ln1", UI_Graph_ADD, 7, UI_Color_White, 3, 710, 540, 1210, 540);
UIGraphRefresh(&referee_recv_info->referee_id, 1, line);
```

### 矩形

```c
void UIRectangleDraw(Graph_Data_t *graph, char graphname[3], uint32_t Graph_Operate,
                     uint32_t Graph_Layer, uint32_t Graph_Color, uint32_t Graph_Width,
                     uint32_t Start_x, uint32_t Start_y, uint32_t End_x, uint32_t End_y);
```

字段含义：

- `(Start_x, Start_y)`：一个顶点。
- `(End_x, End_y)`：对角顶点。

### 整圆

```c
void UICircleDraw(Graph_Data_t *graph, char graphname[3], uint32_t Graph_Operate,
                  uint32_t Graph_Layer, uint32_t Graph_Color, uint32_t Graph_Width,
                  uint32_t Start_x, uint32_t Start_y, uint32_t Graph_Radius);
```

字段含义：

- `(Start_x, Start_y)`：圆心。
- `Graph_Radius`：半径。

### 椭圆

```c
void UIOvalDraw(Graph_Data_t *graph, char graphname[3], uint32_t Graph_Operate,
                uint32_t Graph_Layer, uint32_t Graph_Color, uint32_t Graph_Width,
                uint32_t Start_x, uint32_t Start_y, uint32_t end_x, uint32_t end_y);
```

字段含义：

- `(Start_x, Start_y)`：椭圆中心。
- `end_x` / `end_y`：x/y 半轴长度。

### 圆弧

```c
void UIArcDraw(Graph_Data_t *graph, char graphname[3], uint32_t Graph_Operate,
               uint32_t Graph_Layer, uint32_t Graph_Color,
               uint32_t Graph_StartAngle, uint32_t Graph_EndAngle,
               uint32_t Graph_Width, uint32_t Start_x, uint32_t Start_y,
               uint32_t end_x, uint32_t end_y);
```

字段含义：

- `Graph_StartAngle` / `Graph_EndAngle`：起始角度和终止角度。
- `(Start_x, Start_y)`：圆弧中心。
- `end_x` / `end_y`：x/y 半轴长度。

### 浮点数

```c
void UIFloatDraw(Graph_Data_t *graph, char graphname[3], uint32_t Graph_Operate,
                 uint32_t Graph_Layer, uint32_t Graph_Color,
                 uint32_t Graph_Size, uint32_t Graph_Digit, uint32_t Graph_Width,
                 uint32_t Start_x, uint32_t Start_y, int32_t Graph_Float);
```

字段含义：

- `Graph_Size`：字号，写入 `start_angle`。
- `Graph_Digit`：小数位数，写入 `end_angle`。
- `Graph_Float`：显示值，按裁判系统协议需要传入实际浮点数乘以 1000 后的 `int32_t`。

编码方式：

```c
radius = Graph_Float & 0x3FF;
end_x  = (Graph_Float >> 10) & 0x7FF;
end_y  = (Graph_Float >> 21) & 0x7FF;
```

示例：

```c
Graph_Data_t voltage;
UIFloatDraw(&voltage, "vf1", UI_Graph_ADD, 6, UI_Color_Green, 18, 2, 2, 100, 500, 24.5f * 1000);
UIGraphRefresh(&referee_recv_info->referee_id, 1, voltage);
```

### 整数

```c
void UIIntDraw(Graph_Data_t *graph, char graphname[3], uint32_t Graph_Operate,
               uint32_t Graph_Layer, uint32_t Graph_Color,
               uint32_t Graph_Size, uint32_t Graph_Width,
               uint32_t Start_x, uint32_t Start_y, int32_t Graph_Integer);
```

字段含义：

- `Graph_Size`：字号，写入 `start_angle`。
- `Graph_Integer`：显示整数。

编码方式：

```c
radius = Graph_Integer & 0x3FF;
end_x  = (Graph_Integer >> 10) & 0x7FF;
end_y  = (Graph_Integer >> 21) & 0x7FF;
```

### 字符串

```c
void UICharDraw(String_Data_t *graph, char graphname[3], uint32_t Graph_Operate,
                uint32_t Graph_Layer, uint32_t Graph_Color,
                uint32_t Graph_Size, uint32_t Graph_Width,
                uint32_t Start_x, uint32_t Start_y, char *fmt, ...);
```

字段含义：

- `Graph_Size`：字号，写入 `start_angle`。
- `show_Data[30]`：字符串内容，最大 30 字节。
- `end_angle`：字符串长度，由 `strlen(show_Data)` 自动计算。
- `fmt, ...`：类似 `printf` 的格式化参数。

示例：

```c
String_Data_t state;
UICharDraw(&state, "st1", UI_Graph_ADD, 8, UI_Color_Yellow, 15, 2, 150, 700, "gimbal:%s", "gyro");
UICharRefresh(&referee_recv_info->referee_id, state);
```

注意：当前实现内部使用 `vsprintf()`，不会限制长度；字符串内容应主动控制在 30 字节以内，避免覆盖后续数据。

## 刷新发送接口

### 刷新普通图形

```c
void UIGraphRefresh(referee_id_t *_id, int cnt, ...);
```

规则：

- `cnt` 只能是 `1、2、5、7`。
- 后续可变参数传入 `Graph_Data_t` 变量本体，不是指针。
- 该函数会根据 `cnt` 自动选择 `UI_Data_ID_Draw1/2/5/7`。

示例：

```c
Graph_Data_t a, b;
UILineDraw(&a, "a01", UI_Graph_ADD, 7, UI_Color_White, 2, 100, 100, 300, 100);
UICircleDraw(&b, "b01", UI_Graph_ADD, 7, UI_Color_Green, 2, 500, 500, 50);
UIGraphRefresh(&referee_recv_info->referee_id, 2, a, b);
```

### 刷新字符串

```c
void UICharRefresh(referee_id_t *_id, String_Data_t string_Data);
```

规则：

- 一次只能刷新 1 个 `String_Data_t`。
- 字符串图形使用 `UI_Data_ID_DrawChar = 0x110`。

## 当前工程 UI 示例

`referee_task.c` 中当前绘制了两类 UI：

### 静态准线

```c
UILineDraw(&UI_shoot_line[0], "sl0", UI_Graph_ADD, 7, UI_Color_White, 3, 710, 540, 1210, 540);
UILineDraw(&UI_shoot_line[1], "sl1", UI_Graph_ADD, 7, UI_Color_White, 3, 960, 340, 960, 740);
UIGraphRefresh(&referee_recv_info->referee_id, 5,
               UI_shoot_line[0], UI_shoot_line[1], UI_shoot_line[2], UI_shoot_line[3], UI_shoot_line[4]);
```

### 动态模式文字

初始化时先新增：

```c
UICharDraw(&UI_State_dyn[1], "sd1", UI_Graph_ADD, 8, UI_Color_Yellow, 15, 2, 270, 700, "zeroforce");
UICharRefresh(&referee_recv_info->referee_id, UI_State_dyn[1]);
```

模式变化时修改同名图形：

```c
UICharDraw(&UI_State_dyn[1], "sd1", UI_Graph_Change, 8, UI_Color_Yellow, 15, 2, 270, 700, "   gyro  ");
UICharRefresh(&referee_recv_info->referee_id, UI_State_dyn[1]);
```

## 绘制新 UI 的推荐写法

1. 为每个图形分配固定的 3 字节名称，例如 `"p01"`、`"m01"`、`"v01"`。
2. 初始化阶段清空 UI，并用 `UI_Graph_ADD` 绘制所有静态元素。
3. 动态元素也先 `ADD` 一次，保证后续能 `Change`。
4. 运行阶段只在状态变化时刷新，避免无意义占用裁判系统 10Hz 交互带宽。
5. 同类普通图形尽量按 `1、2、5、7` 个打包发送。
6. 字符串单独用 `UICharRefresh()` 发送。
7. 字符串内容控制在 30 字节以内。

## 常见注意事项

- UI 交互通道上行频率上限为 10Hz，`RefereeSend()` 内部也带延时控制。
- 图层建议提前规划，例如准线用 7 层，状态文字用 8 层，避免互相覆盖。
- 修改图形必须保持 `graphic_name` 不变。
- 删除全部 UI 后，原有图形都需要重新 `UI_Graph_ADD`。
- `UIGraphRefresh()` 的可变参数传结构体值，不传地址。
- `UICharDraw()` 使用格式化字符串时要注意长度，当前代码没有边界检查。
- `UI_Seq` 是全局包序号，新增发送函数时要保持序号递增逻辑一致。
