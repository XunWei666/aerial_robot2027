#include "gimbal.h"
#include "robot_def.h"
#include "dji_motor.h"
#include "ins_task.h"
#include "message_center.h"
#include "general_def.h"
#include "LK4005.h"
#include "bsp_dwt.h"

#define GIMBAL_YAW_DEBUG_LOG_CAPACITY (384U) /**< Yaw 调试日志缓冲区容量 */

/**
 * @brief Yaw 轴调试采样点
 * @note  该结构体用于记录 Yaw 轴连续运动过程中的目标角、反馈角以及串级 PID
 *        内部状态，便于后续通过 OpenOCD/GDB 统一回读分析。
 */
typedef struct
{
    uint64_t timeline_us;       /**< 采样时间戳，单位 us */
    uint8_t gimbal_mode;        /**< 当前云台模式 */
    float yaw_target;           /**< 当前 Yaw 目标角，单位 deg */
    float yaw_imu_angle;        /**< IMU 反馈的 Yaw 角度，单位 deg */
    float yaw_motor_angle;      /**< Yaw 电机编码器 total angle，单位 deg */
    float yaw_gyro_rpm;         /**< Yaw IMU 角速度换算后的 RPM 反馈 */
    float angle_pid_ref;        /**< Yaw 角度环参考量 */
    float angle_pid_measure;    /**< Yaw 角度环测量量 */
    float angle_pid_err;        /**< Yaw 角度环误差 */
    float angle_pid_pout;       /**< Yaw 角度环比例项输出 */
    float angle_pid_iout;       /**< Yaw 角度环积分项输出 */
    float angle_pid_output;     /**< Yaw 角度环最终输出 */
    float speed_pid_ref;        /**< Yaw 速度环参考量 */
    float speed_pid_measure;    /**< Yaw 速度环测量量 */
    float speed_pid_err;        /**< Yaw 速度环误差 */
    float speed_pid_pout;       /**< Yaw 速度环比例项输出 */
    float speed_pid_iout;       /**< Yaw 速度环积分项输出 */
    float speed_pid_output;     /**< Yaw 速度环最终输出 */
} GimbalYawDebugSample_s;

static attitude_t *gimbal_IMU_data; // 云台IMU数据
static DJIMotorInstance *yaw_motor;
static LKMotorInstance  *pitch_motor;

static Publisher_t *gimbal_pub;                   // 云台反馈消息话题
static Subscriber_t *gimbal_sub;                  // cmd控制消息订阅者
static Gimbal_Upload_Data_s gimbal_feedback_data; // 回传给cmd的云台状态信息
static Gimbal_Ctrl_Cmd_s gimbal_cmd_recv;         // 来自cmd的控制信息
static float pitch_angle_imu = 0.0f;
static float pitch_angle_eco = 0.0f;
static float pitch_angle_target = 0.0f;
static float yaw_angle_imu = 0.0f;
static float yaw_angle_eco = 0.0f;
static float yaw_angle_target = 0.0f;
static float yaw_gryo_rpm = 0.0f;
static float pitch_gryo_rpm = 0.0f;
// static volatile uint8_t gimbal_yaw_debug_enable = 1U;
// static volatile uint16_t gimbal_yaw_debug_write_index = 0U;
// static volatile uint16_t gimbal_yaw_debug_sample_count = 0U;
// static volatile uint8_t gimbal_yaw_debug_wrapped = 0U;
// static GimbalYawDebugSample_s gimbal_yaw_debug_log[GIMBAL_YAW_DEBUG_LOG_CAPACITY] = {0};

/**
 * @brief 记录一帧 Yaw 轴调试数据
 * @note  该函数在 GimbalTask() 内周期调用，使用环形缓冲区保存最近一段 Yaw 轴
 *        连续运动数据，供 OpenOCD/GDB 在运动结束后统一读取。
 */
// static void GimbalRecordYawDebugSample(void)
// {
//     GimbalYawDebugSample_s *sample = NULL;
//
//     if ((gimbal_yaw_debug_enable == 0U) || (yaw_motor == NULL))
//     {
//         return;
//     }
//
//     sample = &gimbal_yaw_debug_log[gimbal_yaw_debug_write_index];
//
//     sample->timeline_us = DWT_GetTimeline_us();
//     sample->gimbal_mode = (uint8_t)gimbal_cmd_recv.gimbal_mode;
//     sample->yaw_target = yaw_angle_target;
//     sample->yaw_imu_angle = yaw_angle_imu;
//     sample->yaw_motor_angle = yaw_angle_eco;
//     sample->yaw_gyro_rpm = yaw_gryo_rpm;
//     sample->angle_pid_ref = yaw_motor->motor_controller.angle_PID.Ref;
//     sample->angle_pid_measure = yaw_motor->motor_controller.angle_PID.Measure;
//     sample->angle_pid_err = yaw_motor->motor_controller.angle_PID.Err;
//     sample->angle_pid_pout = yaw_motor->motor_controller.angle_PID.Pout;
//     sample->angle_pid_iout = yaw_motor->motor_controller.angle_PID.Iout;
//     sample->angle_pid_output = yaw_motor->motor_controller.angle_PID.Output;
//     sample->speed_pid_ref = yaw_motor->motor_controller.speed_PID.Ref;
//     sample->speed_pid_measure = yaw_motor->motor_controller.speed_PID.Measure;
//     sample->speed_pid_err = yaw_motor->motor_controller.speed_PID.Err;
//     sample->speed_pid_pout = yaw_motor->motor_controller.speed_PID.Pout;
//     sample->speed_pid_iout = yaw_motor->motor_controller.speed_PID.Iout;
//     sample->speed_pid_output = yaw_motor->motor_controller.speed_PID.Output;
//
//     gimbal_yaw_debug_write_index++;
//     if (gimbal_yaw_debug_write_index >= GIMBAL_YAW_DEBUG_LOG_CAPACITY)
//     {
//         gimbal_yaw_debug_write_index = 0U;
//         gimbal_yaw_debug_wrapped = 1U;
//     }
//
//     if (gimbal_yaw_debug_sample_count < GIMBAL_YAW_DEBUG_LOG_CAPACITY)
//     {
//         gimbal_yaw_debug_sample_count++;
//     }
// }

void GimbalInit()
{   
    gimbal_IMU_data = INS_Init(); // IMU先初始化,获取姿态数据指针赋给yaw电机的其他数据来源
    // YAW
    Motor_Init_Config_s yaw_config = {
        .can_init_config = {
            .can_handle = &hcan1,
            .tx_id = 1,
        },
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 250.0f, // 250
                .Ki = 1.f,   // 1.0
                .Kd = 6.0f,   // 6
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .IntegralLimit = 2200,
                .MaxOut = 5000,
            },
            .speed_PID = {
                .Kp = 6.f,  // 5
                .Ki = 0.f,
                .Kd = 0.f,
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .IntegralLimit = 5000,
                .MaxOut = 11000,
            },
            .other_angle_feedback_ptr = &gimbal_IMU_data->YawTotalAngle,
            // 还需要增加角速度额外反馈指针,注意方向,ins_task.md中有c板的bodyframe坐标系说明
            .other_speed_feedback_ptr = &yaw_gryo_rpm,
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED,
            .speed_feedback_source = MOTOR_FEED,
            .outer_loop_type_temp = ANGLE_LOOP,
            .outer_loop_type_last = ANGLE_LOOP,
            .close_loop_type = ANGLE_LOOP | SPEED_LOOP,
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
            .speed_unit_type = MOTOR_FEEDBACK_RPM,
        },
        .motor_type = GM6020};
    // PITCH
    Motor_Init_Config_s pitch_config = {
        .can_init_config = {
            .can_handle = &hcan1,
            .tx_id = 1,
        },
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 30.f,
                .Ki = 0.1f,
                .Kd = 0.1f,
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement | PID_DerivativeFilter,
                .IntegralLimit = 200,
                .MaxOut = 800,
                .Derivative_LPF_RC = 0.003f,
            },
            .speed_PID = {
                .Kp = 3.0f,
                .Ki = 0.f,
                .Kd = 0.f,
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .IntegralLimit = 800,
                .MaxOut = 1800,
            },
            .other_angle_feedback_ptr = &gimbal_IMU_data->Pitch,
            // 还需要增加角速度额外反馈指针,注意方向,ins_task.md中有c板的bodyframe坐标系说明
            .other_speed_feedback_ptr = (&gimbal_IMU_data->Gyro[0]),
            .gravity = -100.f,
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED,
            .speed_feedback_source = MOTOR_FEED,
            .outer_loop_type_temp = ANGLE_LOOP,
            .outer_loop_type_last = ANGLE_LOOP,
            .close_loop_type = ANGLE_LOOP | SPEED_LOOP,
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
            .speed_unit_type = MOTOR_FEEDBACK_RPM
        },
        .motor_type = LK4005,
    };
    // 电机对total_angle闭环,上电时为零,会保持静止,收到遥控器数据再动
    yaw_motor   = DJIMotorInit(&yaw_config);
    pitch_motor = LKMotorInit(&pitch_config);

    /* 云台消息只保留最新一份，能够避免控制任务消费历史消息而增加时延。 */
    gimbal_pub = PubRegister("gimbal_feed", sizeof(Gimbal_Upload_Data_s));
    gimbal_sub = SubRegister("gimbal_cmd", sizeof(Gimbal_Ctrl_Cmd_s));
}

/* 机器人云台控制核心任务,后续考虑只保留IMU控制,不再需要电机的反馈 */
void GimbalTask()
{
    // 获取云台控制数据
    // 后续增加未收到数据的处理
    SubGetMessage(gimbal_sub, &gimbal_cmd_recv);
    pitch_gryo_rpm = gimbal_IMU_data->Gyro[0] * 9.5493f; // 将陀螺仪角速度rad/s代换为rpm
    yaw_gryo_rpm = gimbal_IMU_data->Gyro[2] * 9.5493f;   // 将陀螺仪角速度rad/s代换为rpm
    switch (gimbal_cmd_recv.gimbal_mode)
    {
    // 停止
    case GIMBAL_ZERO_FORCE:
        DJIMotorStop(yaw_motor);
        LKMotorStop(pitch_motor);
        break;
    // 使用陀螺仪的反馈
    case GIMBAL_GYRO_MODE:
        DJIMotorEnable(yaw_motor);
        LKMotorEnable(pitch_motor);

        //这是因为imu的Yaw轴安装方向错误导致多加一个标志位
        yaw_motor->motor_settings.motor_reverse_flag = gimbal_cmd_recv.is_reverse;

        DJIMotorChangeFeed(yaw_motor, ANGLE_LOOP, IMU_FEED);
        DJIMotorChangeFeed(yaw_motor, SPEED_LOOP, IMU_FEED);
        LKMotorChangeFeed(pitch_motor, ANGLE_LOOP, IMU_FEED);
        LKMotorChangeFeed(pitch_motor, SPEED_LOOP, IMU_FEED);
        DJIMotorSetRef(yaw_motor, gimbal_cmd_recv.yaw); // yaw和pitch会在robot_cmd中处理好多圈和单圈
        LKMotorSetRef(pitch_motor, gimbal_cmd_recv.pitch);
        break;
    // 电机自身编码器反馈
    case GIMBAL_SELF_MODE:
        DJIMotorEnable(yaw_motor);
        LKMotorEnable(pitch_motor);
        yaw_motor->motor_settings.motor_reverse_flag = gimbal_cmd_recv.is_reverse;
        DJIMotorChangeFeed(yaw_motor, ANGLE_LOOP, MOTOR_FEED);
        DJIMotorChangeFeed(yaw_motor, SPEED_LOOP, MOTOR_FEED);
        LKMotorChangeFeed(pitch_motor, ANGLE_LOOP, MOTOR_FEED);
        LKMotorChangeFeed(pitch_motor, SPEED_LOOP, MOTOR_FEED);
        DJIMotorSetRef(yaw_motor, gimbal_cmd_recv.yaw); // yaw和pitch会在robot_cmd中处理好多圈和单圈
        LKMotorSetRef(pitch_motor, gimbal_cmd_recv.pitch);
            break;
    default:
        break;
    }

    // 在合适的地方添加pitch重力补偿前馈力矩
    // 根据IMU姿态/pitch电机角度反馈计算出当前配重下的重力矩
    // ...

    pitch_angle_imu = gimbal_IMU_data->Pitch;
    pitch_angle_eco = pitch_motor->measure.total_angle;
    pitch_angle_target = gimbal_cmd_recv.pitch;
    yaw_angle_imu = gimbal_IMU_data->YawTotalAngle;
    yaw_angle_eco = yaw_motor->measure.total_angle;
    yaw_angle_target = gimbal_cmd_recv.yaw;

    // 设置反馈数据
    gimbal_feedback_data.gimbal_imu_data = *gimbal_IMU_data;
    gimbal_feedback_data.yaw_motor_angle = yaw_motor->measure.total_angle;
    gimbal_feedback_data.pitch_motor_angle = pitch_motor->measure.total_angle;
    gimbal_feedback_data.gimbal_mode = gimbal_cmd_recv.gimbal_mode;
    // GimbalRecordYawDebugSample();

    // 推送消息
    PubPushMessage(gimbal_pub, (void *)&gimbal_feedback_data);
}
