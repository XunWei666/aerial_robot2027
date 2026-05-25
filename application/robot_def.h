/**
 * @file robot_def.h
 * @author NeoZeng neozng1@hnu.edu.cn
 * @author Even
 * @brief 云台单板工程的控制数据定义与机构参数配置
 * @date 2022-12-02
 *
 * @copyright Copyright (c) HNU YueLu EC 2022 all rights reserved
 *
 */
#pragma once
#ifndef ROBOT_DEF_H
#define ROBOT_DEF_H

#include "ins_task.h"
#include "stdint.h"
#include "dji_motor.h"

/* 当前工程固定为云台与发射机构单板控制，不再保留底盘板或双板切换宏。 */

/* 云台参数 */
#define YAW_MAX_ANGLE_CODE 153.f
#define YAW_MIN_ANGLE_CODE 83.f
#define YAW_LIMIT_BAND     3.f
#define PITCH_MAX_ANGLE_CODE 110.f
#define PITCH_MIN_ANGLE_CODE 70.f
#define PITCH_MAX_ANGLE_IMU 30.f
#define PITCH_MIN_ANGLE_IMU -10.f
#define INPUT_LIMIT_BAND    0.001f

/* 发射参数 */
#define LOADER_MAX_CURRENT -7500  // 卡弹检测电流
#define JAM_DETECT_TIME  20  // 卡弹检测时长，根据执行频率来设定，1000 / freq * this = xms 80 ~ 120ms 即可
#define JAM_REVERSE_TIME 30  // 卡弹反转时长，根据执行频率来设定，1000 / freq * this = xms 100 ~ 300ms即可

/* 陀螺仪坐标系相对云台坐标系的方向定义 */
#define GYRO2GIMBAL_DIR_YAW 1
#define GYRO2GIMBAL_DIR_PITCH 1
#define GYRO2GIMBAL_DIR_ROLL 1

#pragma pack(1)

/**
 * @brief 基础控制模式与消息类型定义
 * @note 这些枚举和结构体会作为 robot_cmd 与各应用之间的通信数据。
 */
typedef enum
{
    SYS_ESTOP = 0,    // 急停
    SYS_SAFE_STANDBY, // 安全待机
    SYS_RC_CONTROL,   // 遥控器控制
    SYS_PC_CONTROL    // 键鼠控制
} System_State_e;

typedef enum
{
    YAW_FREE = 0,     // Yaw轴无限制
    YAW_MIN_BLOCKED,  // Yaw轴左限位
    YAW_MAX_BLOCKED   // Yaw轴右限位
} Yaw_Limit_State_e;

typedef enum
{
    GIMBAL_ZERO_FORCE = 0, // 云台零力模式
    GIMBAL_GYRO_MODE,      // 云台陀螺仪反馈模式
    GIMBAL_SELF_MODE       // 云台编码器反馈模式
} gimbal_mode_e;

typedef enum
{
    FRICTION_OFF = 0, // 摩擦轮关闭
    FRICTION_ON       // 摩擦轮开启
} friction_mode_e;

typedef enum
{
    NORMAL_FREQ = 12,
    HIGHER_FREQ = 15,
    MAX_FREQ = 20,
} shoot_freq_e;

typedef enum
{
    BULLET_SPEED_NONE = 0,
    SMALL_AMU_16 = 16,
    SMALL_AMU_17 = 17,
    SMALL_AMU_18 = 18
} Bullet_Speed_e;

typedef enum
{
    LOAD_STOP = 0,  // 停止发射
    LOAD_REVERSE,   // 反转退弹
    LOAD_1_BULLET,  // 单发
    LOAD_3_BULLET,  // 三连发
    LOAD_BURSTFIRE, // 连发
} loader_mode_e;

/* ----------------CMD 应用发布的控制数据，应由 gimbal / shoot 订阅---------------- */

typedef struct
{
    float yaw;                       // 云台 yaw 目标值
    float pitch;                     // 云台 pitch 目标值
    Motor_Reverse_Flag_e is_reverse; // 云台跟随方向
    gimbal_mode_e gimbal_mode;       // 云台工作模式
} Gimbal_Ctrl_Cmd_s;

typedef struct
{
    shoot_freq_e shoot_freq;       // 连发频率
    loader_mode_e load_mode;       // 拨弹机构模式
    friction_mode_e friction_mode; // 摩擦轮模式
    Bullet_Speed_e bullet_speed;   // 弹速枚举
    uint8_t rest_heat;             // 剩余热量
} Shoot_Ctrl_Cmd_s;

/* ----------------gimbal / shoot 发布的反馈数据---------------- */

typedef struct
{
    attitude_t gimbal_imu_data; // 云台 IMU 解算结果
    float yaw_motor_angle;      // yaw 电机角度反馈
    float pitch_motor_angle;    // pitch 电机角度反馈
    gimbal_mode_e gimbal_mode;  // 当前云台模式
} Gimbal_Upload_Data_s;

typedef struct
{
    DJI_Motor_Measure_s loader_data; // 拨弹电机反馈
} Shoot_Upload_Data_s;

#pragma pack()

#endif // ROBOT_DEF_H
