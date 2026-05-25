/**
 * @file    picture_transmission.c
 * @brief   图传链路遥控输入解析模块实现
 * @note    该文件解析 RoboMaster 图传链路 21 字节遥控数据帧，并维护链路在线状态。
 */

#include "picture_transmission.h"

#include <string.h>

#include "bsp_log.h"
#include "bsp_usart.h"
#include "daemon.h"

#define PICTURE_TRANSMISSION_FRAME_SIZE 21             /**< 图传链路遥控数据帧长度 */
#define PICTURE_TRANSMISSION_SOF_1 0xA9                /**< 图传链路数据帧头第 1 字节 */
#define PICTURE_TRANSMISSION_SOF_2 0x53                /**< 图传链路数据帧头第 2 字节 */
#define PICTURE_TRANSMISSION_CHANNEL_OFFSET 1024       /**< 摇杆和拨轮通道中位偏置 */
#define PICTURE_TRANSMISSION_CHANNEL_LIMIT 660         /**< 摇杆和拨轮通道有效幅值上限 */
#define PICTURE_TRANSMISSION_LONG_PRESS_TICK_THRESH 30 /**< 键盘长按判定帧数阈值 */
#define PICTURE_TRANSMISSION_DAEMON_RELOAD_COUNT 10    /**< 图传链路离线守护重载计数 */

#define PICTURE_TRANSMISSION_BITFIELD_BYTE_OFFSET 2     /**< 通道位域区起始字节偏移 */
#define PICTURE_TRANSMISSION_MOUSE_BYTE_OFFSET 10        /**< 鼠标三轴数据起始字节偏移 */
#define PICTURE_TRANSMISSION_MOUSE_BUTTON_BYTE_OFFSET 16 /**< 鼠标按键数据字节偏移 */
#define PICTURE_TRANSMISSION_KEY_BYTE_OFFSET 17          /**< 键盘位图起始字节偏移 */

/**
 * @brief 图传数据帧位域起始 bit 偏移
 */
typedef enum
{
    PT_FRAME_CH0_BIT = 0,
    PT_FRAME_CH1_BIT = 11,
    PT_FRAME_CH2_BIT = 22,
    PT_FRAME_CH3_BIT = 33,
    PT_FRAME_MODE_SW_BIT = 44,
    PT_FRAME_PAUSE_BIT = 46,
    PT_FRAME_FN_1_BIT = 47,
    PT_FRAME_FN_2_BIT = 48,
    PT_FRAME_WHEEL_BIT = 49,
    PT_FRAME_TRIGGER_BIT = 60
} PictureTransmissionFrameBit_e;

static PictureTransmissionCtrl_s pt_ctrl[2]; /**< 图传链路解析数据双缓冲 */
static uint8_t pt_init_flag = 0;              /**< 图传链路模块初始化标志 */
static USARTInstance *pt_usart_instance;      /**< 图传链路 USART 实例 */
static DaemonInstance *pt_daemon_instance;    /**< 图传链路离线守护实例 */

static const uint16_t pt_crc16_tab[256] = { /**< 图传协议 CRC16 查表 */
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/**
 * @brief 计算 CRC16 校验值
 * @param data 待校验数据首地址
 * @param len 数据长度，单位为字节
 * @param init_crc CRC16 初始值
 * @retval 返回计算得到的 CRC16
 */
static uint16_t PTGetCRC16(const uint8_t *data, uint16_t len, uint16_t init_crc)
{
    uint16_t crc = init_crc;

    if (data == NULL)
    {
        return 0xffff;
    }

    while (len-- > 0)
    {
        /* 协议示例采用低字节查表方式，保持与官方 CRC16 示例一致。 */
        crc = (uint16_t)((crc >> 8) ^ pt_crc16_tab[(crc ^ *data++) & 0x00ff]);
    }

    return crc;
}

/**
 * @brief 校验图传链路数据帧 CRC16
 * @param frame 图传链路原始数据帧
 * @retval 1 CRC 校验通过
 * @retval 0 CRC 校验失败
 */
static uint8_t PTVerifyCRC16(const uint8_t *frame)
{
    uint16_t expected_crc;

    if (frame == NULL)
    {
        return 0;
    }

    /* 帧尾两个字节为小端 CRC16，本地计算时不包含这两个校验字节。 */
    expected_crc = PTGetCRC16(frame, PICTURE_TRANSMISSION_FRAME_SIZE - 2, 0xffff);
    if (((expected_crc & 0x00ff) == frame[PICTURE_TRANSMISSION_FRAME_SIZE - 2]) &&
        (((expected_crc >> 8) & 0x00ffu) == frame[PICTURE_TRANSMISSION_FRAME_SIZE - 1]))
    {
        return 1;
    }

    return 0;
}

/**
 * @brief 按小端 bit 序从图传帧中提取无符号字段
 * @param frame 图传链路原始数据帧
 * @param bit_offset 相对图传位域区起点的 bit 偏移
 * @param bit_width 字段 bit 宽度
 * @retval 返回提取得到的无符号字段值
 */
static uint16_t PTExtractBits(const uint8_t *frame, uint8_t bit_offset, uint8_t bit_width)
{
    uint16_t value = 0;

    for (uint8_t i = 0; i < bit_width; ++i)
    {
        /* 手动按 bit 解包，避免 packed 位域在不同编译器或位序规则下产生歧义。 */
        uint8_t absolute_bit = (uint8_t)(bit_offset + i);
        uint8_t byte_index = (uint8_t)(PICTURE_TRANSMISSION_BITFIELD_BYTE_OFFSET + absolute_bit / 8);
        uint8_t bit_index = (uint8_t)(absolute_bit % 8);

        if (((frame[byte_index] >> bit_index) & 0x01) != 0)
        {
            value |= (uint16_t)(1 << i);
        }
    }

    return value;
}

/**
 * @brief 将原始 11bit 通道转换成以 0 为中位的控制量
 * @param raw_channel 图传帧中的原始通道值
 * @retval 返回中位偏移后的通道值
 */
static int16_t PTNormalizeChannel(uint16_t raw_channel)
{
    return (int16_t)((int32_t)raw_channel - PICTURE_TRANSMISSION_CHANNEL_OFFSET);
}

/**
 * @brief 限制摇杆通道异常值
 * @param channel 待限制的通道值指针
 */
static void PTRectifyChannel(int16_t *channel)
{
    if ((*channel > PICTURE_TRANSMISSION_CHANNEL_LIMIT) ||
        (*channel < -PICTURE_TRANSMISSION_CHANNEL_LIMIT))
    {
        /* 超出协议正常范围时认为该帧通道异常，置零避免错误输入继续向上层传播。 */
        *channel = 0;
    }
}

/**
 * @brief 限制所有图传摇杆和拨轮通道异常值
 */
static void PTRectifyRCChannels(void)
{
    PTRectifyChannel(&pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.rocker_l_);
    PTRectifyChannel(&pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.rocker_l1);
    PTRectifyChannel(&pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.rocker_r_);
    PTRectifyChannel(&pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.rocker_r1);
    PTRectifyChannel(&pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.wheel);
}

/**
 * @brief 从两个小端字节解析 int16_t
 * @param low 低字节
 * @param high 高字节
 * @retval 返回组合得到的有符号 16bit 数据
 */
static int16_t PTReadInt16LE(uint8_t low, uint8_t high)
{
    return (int16_t)((uint16_t)low | ((uint16_t)high << 8u));
}

/**
 * @brief 根据当前键盘位图生成普通按键层和组合键层
 * @param key_bits 当前图传键盘位图
 */
static void PTUpdateKeyLayers(uint16_t key_bits)
{
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_bits[PICTURE_TRANSMISSION_KEY_PRESS] = key_bits;

    if ((key_bits & (1u << PICTURE_TRANSMISSION_KEY_CTRL)) != 0u)
    {
        /* 组合键层保存按住 Ctrl 时的完整键盘位图，便于上层直接查询 Ctrl+按键。 */
        pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_bits[PICTURE_TRANSMISSION_KEY_WITH_CTRL] = key_bits;
    }
    else
    {
        pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_bits[PICTURE_TRANSMISSION_KEY_WITH_CTRL] = 0u;
    }

    if ((key_bits & (1u << PICTURE_TRANSMISSION_KEY_SHIFT)) != 0u)
    {
        /* Shift 组合键与 Ctrl 组合键分层保存，避免普通按键计数和组合键计数互相污染。 */
        pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_bits[PICTURE_TRANSMISSION_KEY_WITH_SHIFT] = key_bits;
    }
    else
    {
        pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_bits[PICTURE_TRANSMISSION_KEY_WITH_SHIFT] = 0u;
    }
}

/**
 * @brief 更新图传键盘按键上升沿计数
 */
static void PTUpdateKeyCount(void)
{
    uint16_t key_now = pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_bits[PICTURE_TRANSMISSION_KEY_PRESS];
    uint16_t key_last = pt_ctrl[PICTURE_TRANSMISSION_LAST].key_bits[PICTURE_TRANSMISSION_KEY_PRESS];
    uint16_t key_with_ctrl = pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_bits[PICTURE_TRANSMISSION_KEY_WITH_CTRL];
    uint16_t key_with_shift = pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_bits[PICTURE_TRANSMISSION_KEY_WITH_SHIFT];
    uint16_t key_last_with_ctrl = pt_ctrl[PICTURE_TRANSMISSION_LAST].key_bits[PICTURE_TRANSMISSION_KEY_WITH_CTRL];
    uint16_t key_last_with_shift = pt_ctrl[PICTURE_TRANSMISSION_LAST].key_bits[PICTURE_TRANSMISSION_KEY_WITH_SHIFT];

    for (uint8_t i = 0u; i < PICTURE_TRANSMISSION_KEY_COUNT; ++i)
    {
        uint16_t key_mask = (uint16_t)(1u << i);

        if ((i == PICTURE_TRANSMISSION_KEY_SHIFT) || (i == PICTURE_TRANSMISSION_KEY_CTRL))
        {
            /* Ctrl 和 Shift 作为修饰键使用，不参与普通按键触发计数。 */
            continue;
        }

        if (((key_now & key_mask) != 0u) && ((key_last & key_mask) == 0u) &&
            ((key_with_ctrl & key_mask) == 0u) && ((key_with_shift & key_mask) == 0u))
        {
            /* 普通按键仅在未被 Ctrl 或 Shift 修饰时记录一次上升沿。 */
            pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_count[PICTURE_TRANSMISSION_KEY_PRESS][i]++;
        }

        if (((key_with_ctrl & key_mask) != 0u) && ((key_last_with_ctrl & key_mask) == 0u))
        {
            /* Ctrl 组合键按下瞬间单独计数，方便上层实现切换类操作。 */
            pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_count[PICTURE_TRANSMISSION_KEY_WITH_CTRL][i]++;
        }

        if (((key_with_shift & key_mask) != 0u) && ((key_last_with_shift & key_mask) == 0u))
        {
            /* Shift 组合键按下瞬间单独计数，语义与普通按键层解耦。 */
            pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_count[PICTURE_TRANSMISSION_KEY_WITH_SHIFT][i]++;
        }
    }
}

/**
 * @brief 更新图传键盘单键状态机
 */
static void PTUpdateKeyState(void)
{
    uint16_t key_now = pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_bits[PICTURE_TRANSMISSION_KEY_PRESS];

    for (uint8_t i = 0; i < PICTURE_TRANSMISSION_KEY_COUNT; ++i)
    {
        uint8_t is_down = (uint8_t)((key_now >> i) & 0x01);
        PictureTransmissionKeyState_s *last_state = &pt_ctrl[PICTURE_TRANSMISSION_LAST].key_state[i];
        PictureTransmissionKeyState_s *curr_state = &pt_ctrl[PICTURE_TRANSMISSION_TEMP].key_state[i];

        if (is_down != 0)
        {
            /* 物理按键按下时，根据上一帧状态区分按下沿、短按持续和长按持续。 */
            if ((last_state->state == PICTURE_TRANSMISSION_KEY_RELEASED) ||
                (last_state->state == PICTURE_TRANSMISSION_KEY_PRESS_UP))
            {
                curr_state->state = PICTURE_TRANSMISSION_KEY_PRESS_DOWN;
                curr_state->hold_tick = 0;
            }
            else
            {
                curr_state->hold_tick = (uint16_t)(last_state->hold_tick + 1);
                if (curr_state->hold_tick > PICTURE_TRANSMISSION_LONG_PRESS_TICK_THRESH)
                {
                    curr_state->state = PICTURE_TRANSMISSION_KEY_LONG_PRESS;
                }
                else
                {
                    curr_state->state = PICTURE_TRANSMISSION_KEY_PRESSING;
                }
            }
        }
        else
        {
            /* 物理按键松开时保留最后 hold_tick，供上层区分短按释放和长按释放。 */
            if ((last_state->state != PICTURE_TRANSMISSION_KEY_RELEASED) &&
                (last_state->state != PICTURE_TRANSMISSION_KEY_PRESS_UP))
            {
                curr_state->state = PICTURE_TRANSMISSION_KEY_PRESS_UP;
                curr_state->hold_tick = last_state->hold_tick;
            }
            else
            {
                curr_state->state = PICTURE_TRANSMISSION_KEY_RELEASED;
                curr_state->hold_tick = 0u;
            }
        }
    }
}

/**
 * @brief 解析图传链路 21 字节遥控数据帧
 * @param frame 图传链路原始数据帧
 * @retval 1 解析成功
 * @retval 0 帧头或 CRC 校验失败
 */
static uint8_t PTDecodeFrame(const uint8_t *frame)
{
    uint8_t mouse_button;
    uint16_t key_bits;
    uint32_t valid_frame_count;

    if ((frame == NULL) ||
        (frame[0] != PICTURE_TRANSMISSION_SOF_1) ||
        (frame[1] != PICTURE_TRANSMISSION_SOF_2) ||
        (PTVerifyCRC16(frame) == 0))
    {
        /* 帧头或 CRC 不合法时直接丢弃本帧，保留上一帧有效控制输入。 */
        return 0;
    }

    /* 解析新帧前先保存上一帧，供上层与本帧做 TEMP/LAST 边沿比较。 */
    memcpy(&pt_ctrl[PICTURE_TRANSMISSION_LAST], &pt_ctrl[PICTURE_TRANSMISSION_TEMP],
           sizeof(PictureTransmissionCtrl_s));

    valid_frame_count = pt_ctrl[PICTURE_TRANSMISSION_TEMP].valid_frame_count + 1u;

    /* 前 8 字节位域区包含 4 个摇杆通道、模式键、功能键、拨轮和扳机键。 */
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.rocker_r_ =
        PTNormalizeChannel(PTExtractBits(frame, PT_FRAME_CH0_BIT, 11));
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.rocker_r1 =
        PTNormalizeChannel(PTExtractBits(frame, PT_FRAME_CH1_BIT, 11));
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.rocker_l1 =
        PTNormalizeChannel(PTExtractBits(frame, PT_FRAME_CH2_BIT, 11));
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.rocker_l_ =
        PTNormalizeChannel(PTExtractBits(frame, PT_FRAME_CH3_BIT, 11));
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.wheel =
        PTNormalizeChannel(PTExtractBits(frame, PT_FRAME_WHEEL_BIT, 11));
    PTRectifyRCChannels();

    pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.mode_sw =
        (uint8_t)PTExtractBits(frame, PT_FRAME_MODE_SW_BIT, 2);
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.pause =
        (uint8_t)PTExtractBits(frame, PT_FRAME_PAUSE_BIT, 1);
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.fn_1 =
        (uint8_t)PTExtractBits(frame, PT_FRAME_FN_1_BIT, 1);
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.fn_2 =
        (uint8_t)PTExtractBits(frame, PT_FRAME_FN_2_BIT, 1);
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].rc.trigger =
        (uint8_t)PTExtractBits(frame, PT_FRAME_TRIGGER_BIT, 1);

    /* 鼠标三轴数据按 int16_t 小端字段传输。 */
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].mouse.x =
        PTReadInt16LE(frame[PICTURE_TRANSMISSION_MOUSE_BYTE_OFFSET],
                      frame[PICTURE_TRANSMISSION_MOUSE_BYTE_OFFSET + 1]);
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].mouse.y =
        PTReadInt16LE(frame[PICTURE_TRANSMISSION_MOUSE_BYTE_OFFSET + 2],
                      frame[PICTURE_TRANSMISSION_MOUSE_BYTE_OFFSET + 3]);
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].mouse.z =
        PTReadInt16LE(frame[PICTURE_TRANSMISSION_MOUSE_BYTE_OFFSET + 4],
                      frame[PICTURE_TRANSMISSION_MOUSE_BYTE_OFFSET + 5]);

    mouse_button = frame[PICTURE_TRANSMISSION_MOUSE_BUTTON_BYTE_OFFSET];
    /* 鼠标三键各占 2bit，当前直接保留协议原始状态值。 */
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].mouse.press_left = (uint8_t)(mouse_button & 0x03);
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].mouse.press_right = (uint8_t)((mouse_button >> 2) & 0x03);
    pt_ctrl[PICTURE_TRANSMISSION_TEMP].mouse.press_middle = (uint8_t)((mouse_button >> 4) & 0x03);

    key_bits = (uint16_t)frame[PICTURE_TRANSMISSION_KEY_BYTE_OFFSET] |
               ((uint16_t)frame[PICTURE_TRANSMISSION_KEY_BYTE_OFFSET + 1] << 8);
    /* 键盘输入先更新位图层，再基于 TEMP/LAST 计算边沿计数和单键状态机。 */
    PTUpdateKeyLayers(key_bits);
    PTUpdateKeyCount();
    PTUpdateKeyState();

    pt_ctrl[PICTURE_TRANSMISSION_TEMP].valid_frame_count = valid_frame_count;

    return 1;
}

/**
 * @brief 图传链路串口接收回调
 */
static void PictureTransmissionRxCallback(void)
{
    if (PTDecodeFrame(pt_usart_instance->recv_buff) != 0)
    {
        /* 仅在收到完整且校验通过的图传帧时喂狗，避免噪声帧错误维持在线状态。 */
        DaemonReload(pt_daemon_instance);
    }
}

/**
 * @brief 图传链路离线回调
 * @param id 守护实例拥有者指针，本模块不使用
 */
static void PictureTransmissionLostCallback(void *id)
{
    (void)id;
    /* 离线时清空输入，避免上层继续使用最后一帧有效遥控数据。 */
    memset(pt_ctrl, 0, sizeof(pt_ctrl));
    USARTServiceInit(pt_usart_instance);
    LOGWARNING("[picture_transmission] remote input lost");
}

/**
 * @brief 初始化图传链路遥控解析模块
 * @param pt_usart_handle 图传链路使用的串口句柄
 * @retval 返回图传链路双缓冲解析数据首地址
 * @note  图传链路串口应配置为 921600-8-N-1，无硬件流控。
 */
PictureTransmissionCtrl_s *PictureTransmissionInit(UART_HandleTypeDef *pt_usart_handle)
{
    USART_Init_Config_s usart_conf = {
        .recv_buff_size = PICTURE_TRANSMISSION_FRAME_SIZE,
        .usart_handle = pt_usart_handle,
        .module_callback = PictureTransmissionRxCallback,
    };
    Daemon_Init_Config_s daemon_conf = {
        .reload_count = PICTURE_TRANSMISSION_DAEMON_RELOAD_COUNT,
        .callback = PictureTransmissionLostCallback,
        .owner_id = NULL,
    };

    /* 图传链路复用 bsp_usart 的 DMA + Idle 接收框架，每收到一帧后回调解析。 */
    pt_usart_instance = USARTRegister(&usart_conf);
    /* 图传有效帧周期性到达时会刷新 daemon，超时后由回调清空输入。 */
    pt_daemon_instance = DaemonRegister(&daemon_conf);
    pt_init_flag = 1;

    return pt_ctrl;
}

/**
 * @brief 查询图传链路遥控输入是否在线
 * @retval 1 图传链路在线
 * @retval 0 图传链路离线或尚未初始化
 */
uint8_t PictureTransmissionIsOnline(void)
{
    if (pt_init_flag != 0)
    {
        return DaemonIsOnline(pt_daemon_instance);
    }

    return 0;
}
