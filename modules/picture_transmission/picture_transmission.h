/**
 * @file    picture_transmission.h
 *
 * @brief   图传链路遥控输入解析模块接口
 *
 * @note    该模块只负责图传链路下发数据帧的解析与在线状态维护，不参与控制源选择。
 */

#ifndef PICTURE_TRANSMISSION_H
#define PICTURE_TRANSMISSION_H

#include <stdint.h>

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 图传链路双缓冲索引
 */
typedef enum
{
    PICTURE_TRANSMISSION_TEMP = 0u, /**< 当前最新解析结果 */
    PICTURE_TRANSMISSION_LAST = 1u  /**< 上一帧解析结果 */
} PictureTransmissionBufferIndex_e;

/**
 * @brief 图传键盘组合层索引
 */
typedef enum
{
    PICTURE_TRANSMISSION_KEY_PRESS = 0,      /**< 不带修饰键的普通按键层 */
    PICTURE_TRANSMISSION_KEY_WITH_CTRL = 1,  /**< Ctrl 组合键层 */
    PICTURE_TRANSMISSION_KEY_WITH_SHIFT = 2, /**< Shift 组合键层 */
    PICTURE_TRANSMISSION_KEY_LAYER_COUNT
} PictureTransmissionKeyLayer_e;

/**
 * @brief 图传键盘按键 bit 索引
 * @note  该顺序来自图传链路键盘位图，A/D 顺序与 DBUS 遥控器旧结构不同。
 */
typedef enum
{
    PICTURE_TRANSMISSION_KEY_W = 0,
    PICTURE_TRANSMISSION_KEY_S,
    PICTURE_TRANSMISSION_KEY_A,
    PICTURE_TRANSMISSION_KEY_D,
    PICTURE_TRANSMISSION_KEY_SHIFT,
    PICTURE_TRANSMISSION_KEY_CTRL,
    PICTURE_TRANSMISSION_KEY_Q,
    PICTURE_TRANSMISSION_KEY_E,
    PICTURE_TRANSMISSION_KEY_R,
    PICTURE_TRANSMISSION_KEY_F,
    PICTURE_TRANSMISSION_KEY_G,
    PICTURE_TRANSMISSION_KEY_Z,
    PICTURE_TRANSMISSION_KEY_X,
    PICTURE_TRANSMISSION_KEY_C,
    PICTURE_TRANSMISSION_KEY_V,
    PICTURE_TRANSMISSION_KEY_B,
    PICTURE_TRANSMISSION_KEY_COUNT
} PictureTransmissionKey_e;

/**
 * @brief 图传键盘单键状态
 */
typedef enum
{
    PICTURE_TRANSMISSION_KEY_RELEASED = 0, /**< 持续松开 */
    PICTURE_TRANSMISSION_KEY_PRESS_DOWN,    /**< 本帧刚按下 */
    PICTURE_TRANSMISSION_KEY_PRESSING,      /**< 持续按下但未达到长按阈值 */
    PICTURE_TRANSMISSION_KEY_LONG_PRESS,    /**< 持续按下且达到长按阈值 */
    PICTURE_TRANSMISSION_KEY_PRESS_UP       /**< 本帧刚松开 */
} PictureTransmissionKeyState_e;

/**
 * @brief 图传键盘单键状态机数据
 */
typedef struct
{
    PictureTransmissionKeyState_e state; /**< 当前按键状态 */
    uint16_t hold_tick;                  /**< 当前按下持续帧数 */
} PictureTransmissionKeyState_s;

/**
 * @brief 图传遥控器按键与拨杆输入数据
 */
typedef struct
{
    int16_t rocker_l_; /**< 左摇杆水平通道 */
    int16_t rocker_l1; /**< 左摇杆竖直通道 */
    int16_t rocker_r_; /**< 右摇杆水平通道 */
    int16_t rocker_r1; /**< 右摇杆竖直通道 */
    int16_t wheel;     /**< 侧边拨轮通道 */
    uint8_t mode_sw;   /**< 图传遥控器模式开关 */
    uint8_t pause;     /**< 图传遥控器暂停键 */
    uint8_t fn_1;      /**< 图传遥控器自定义功能键 1 */
    uint8_t fn_2;      /**< 图传遥控器自定义功能键 2 */
    uint8_t trigger;   /**< 图传遥控器扳机键 */
} PictureTransmissionRc_s;

/**
 * @brief 图传链路鼠标输入数据
 */
typedef struct
{
    int16_t x;            /**< 鼠标 X 轴相对位移 */
    int16_t y;            /**< 鼠标 Y 轴相对位移 */
    int16_t z;            /**< 鼠标滚轮或 Z 轴相对位移 */
    uint8_t press_left;   /**< 鼠标左键状态 */
    uint8_t press_right;  /**< 鼠标右键状态 */
    uint8_t press_middle; /**< 鼠标中键状态 */
} PictureTransmissionMouse_s;

/**
 * @brief 图传链路解析后的完整遥控输入数据
 */
typedef struct
{
    PictureTransmissionRc_s rc;                                      /**< 遥控器实体输入 */
    PictureTransmissionMouse_s mouse;                                /**< 鼠标输入 */
    uint16_t key_bits[PICTURE_TRANSMISSION_KEY_LAYER_COUNT];         /**< 键盘位图与组合键位图 */
    uint8_t key_count[PICTURE_TRANSMISSION_KEY_LAYER_COUNT]
                     [PICTURE_TRANSMISSION_KEY_COUNT];               /**< 按键上升沿计数 */
    PictureTransmissionKeyState_s key_state[PICTURE_TRANSMISSION_KEY_COUNT]; /**< 单键状态机 */
    uint32_t valid_frame_count;                                      /**< 成功解析的有效帧计数 */
} PictureTransmissionCtrl_s;

/**
 * @brief 初始化图传链路遥控解析模块
 * @param pt_usart_handle 图传链路使用的串口句柄
 * @retval 返回图传链路双缓冲解析数据首地址
 * @note  图传链路串口应配置为 921600-8-N-1，无硬件流控。
 */
PictureTransmissionCtrl_s *PictureTransmissionInit(UART_HandleTypeDef *pt_usart_handle);

/**
 * @brief 查询图传链路遥控输入是否在线
 * @retval 1 图传链路在线
 * @retval 0 图传链路离线或尚未初始化
 */
uint8_t PictureTransmissionIsOnline(void);

#ifdef __cplusplus
}
#endif

#endif /* PICTURE_TRANSMISSION_H */
